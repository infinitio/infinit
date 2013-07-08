#include "NetworkManager.hh"

#include "binary_config.hh"
#include "gap.h"
#include "metrics.hh"

#include <common/common.hh>

#include <hole/Passport.hh>
#include <hole/implementations/slug/Manifest.hh>
#include <hole/storage/Directory.hh>

// This should be outside.
#include <lune/Descriptor.hh>
#include <lune/Identity.hh>
#include <lune/Lune.hh>
#include <lune/Phrase.hh>
#include <lune/Set.hh>

#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Genre.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Trait.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Porcupine.hh>

#include <protocol/Serializer.hh>
#include <protocol/ChanneledStream.hh>

#include <reactor/network/tcp-socket.hh>
#include <reactor/sleep.hh>

#include <elle/format/json.hh>
#include <elle/container/vector.hh>
#include <elle/io/Piece.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/system/Process.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.NetworkManager");

namespace
{
  struct NetworkBlocks
  {
    elle::io::Unique root_block;
    elle::io::Unique root_address;
    elle::io::Unique group_block;
    elle::io::Unique group_address;
  };

  NetworkBlocks
  create_network_root_block(std::string const& id,
                            std::string const& identity_)
  {
    ELLE_TRACE_FUNCTION(id, identity_);

    // XXX this value depends on the network policy and openness.
    static nucleus::neutron::Permissions permissions =
      nucleus::neutron::permissions::read;

    auto e = elle::Status::Error;
    auto genreDirectory = nucleus::neutron::Genre::directory;

    ELLE_DEBUG("Create proton network from id '%s'.", id);
    nucleus::proton::Network network(id);

    //- identity ---------------------------------------------------------------
    lune::Identity        identity;
    if (identity.Restore(identity_) == e)
      throw elle::Exception("Couldn't restore the identity.");

    //- group ------------------------------------------------------------------
    nucleus::neutron::Group group(network, identity.pair().K(), "everybody");
    group.seal(identity.pair().k());

    //- group address ----------------------------------------------------------
    nucleus::proton::Address  group_address(group.bind());

    //- subject ----------------------------------------------------------------
    nucleus::neutron::Subject subject;
    if (subject.Create(group_address) == elle::Status::Error)
      throw elle::Exception("unable to create the group subject");

    //- access------------------------------------------------------------------
    nucleus::proton::Porcupine<nucleus::neutron::Access> access_porcupine{
      nucleus::proton::nest::none()};

    nucleus::proton::Door<nucleus::neutron::Access> access_door =
      access_porcupine.lookup(subject);

    access_door.open();
    access_door().insert(new nucleus::neutron::Record{subject, permissions});
    access_door.close();
    access_porcupine.update(subject);

    // XXX[cf: etoile/automaton/Access.hh>, until no longer encrypted]
#define ACCESS_SECRET_KEY_LENGTH 256
#define ACCESS_SECRET_KEY "no-secret-key"

    // XXX
    static cryptography::SecretKey secret_key(
      cryptography::cipher::Algorithm::aes256,
      ACCESS_SECRET_KEY);

    ELLE_ASSERT_EQ(access_porcupine.strategy(),
                   nucleus::proton::Strategy::value);

    cryptography::Digest access_fingerprint =
      nucleus::neutron::access::fingerprint(access_porcupine);

    nucleus::proton::Radix access_radix = access_porcupine.seal(secret_key);

    //- directory --------------------------------------------------------------
    nucleus::neutron::Object directory{
      network,
      identity.pair().K(),
      genreDirectory
    };

    if (directory.Update(directory.author(),
                         directory.contents(),
                         directory.size(),
                         access_radix,
                         directory.owner_token()) == e)
      throw elle::Exception("unable to update the directory");

    if (directory.Seal(identity.pair().k(), access_fingerprint) == e)
      throw elle::Exception("Cannot seal the access");

    //- directory address ------------------------------------------------------
    nucleus::proton::Address  directory_address(directory.bind());

    NetworkBlocks nb;
    using elle::serialize::to_string;
    using OutputArchive = elle::serialize::OutputBase64Archive;
    to_string<OutputArchive>(nb.root_block) << directory;
    to_string<OutputArchive>(nb.root_address) << directory_address;
    to_string<OutputArchive>(nb.group_block) << group;
    to_string<OutputArchive>(nb.group_address) << group_address;

    ELLE_DEBUG("root block %s", nb.root_block);
    ELLE_DEBUG("root address %s", nb.root_address);
    ELLE_DEBUG("group block %s", nb.group_block);
    ELLE_DEBUG("group address %s", nb.group_address);

    return nb;
  }
}

namespace surface
{
  namespace gap
  {
    namespace json = elle::format::json;
    using MKey = elle::metrics::Key;

    NetworkManager::NetworkManager(plasma::meta::Client& meta,
                                   elle::metrics::Reporter& reporter,
                                   elle::metrics::Reporter& google_reporter,
                                   SelfGetter const& self,
                                   DeviceGetter const& device):
      _meta(meta),
      _reporter(reporter),
      _google_reporter(google_reporter),
      _self{self},
      _device(device),
      _infinit_instance_manager{
        _self().id,
        this->_meta.host(),
        this->_meta.port(),
        this->_meta.token(),
      }
    {
      ELLE_TRACE_METHOD("");
    }

    NetworkManager::~NetworkManager()
    {
      ELLE_TRACE_METHOD("");

      this->clear();
    }

    void
    NetworkManager::clear()
    {
      ELLE_TRACE_METHOD("");

      // XXX.
      this->_infinit_instance_manager.clear();
    }

    std::string
    NetworkManager::create(std::string const& name,
                           bool auto_add)
    {
      ELLE_TRACE_METHOD(name, auto_add);

      this->_reporter.store("network_create_attempt");
      this->_google_reporter.store("network:create:attempt");

      plasma::meta::CreateNetworkResponse response;
      try
      {
        response = this->_meta.create_network(name);
      }
      CATCH_FAILURE_TO_METRICS("network_create");

      this->_reporter.store("network_create_succeed",
                            {{MKey::value, response.created_network_id}});

      this->_google_reporter.store("network:create:succeed");

      // XXX: Device manager.
      if (auto_add)
        this->_meta.network_add_device(response.created_network_id,
                                       this->_device().id);

      return response.created_network_id;
    }

    void
    NetworkManager::prepare(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);

      std::string const network_dir = common::infinit::network_directory(
        this->_self().id,
        network_id);

      ELLE_DEBUG("network directory: %s", network_dir);
      if (!elle::os::path::exists(network_dir))
        elle::os::path::make_path(network_dir);

      std::string const description_filename =
        common::infinit::descriptor_path(this->_self().id, network_id);

      ELLE_DEBUG("descriptor path: %s", description_filename);

      if (!elle::os::path::exists(description_filename))
      {
        // XXX: One or sync?
        if (this->one(network_id).descriptor.empty())
        {
          auto nb = create_network_root_block(network_id,
                                              this->_meta.identity());

          this->_meta.update_network(network_id,
                                     nullptr,
                                     &nb.root_block,
                                     &nb.root_address,
                                     &nb.group_block,
                                     &nb.group_address);

          this->sync(network_id);
        }
      }
      else
      {
        ELLE_DEBUG("network %s already prepared", network_id);
      }
    }

    void
    NetworkManager::to_directory(std::string const& network_id,
                                 std::string const& path)
    {
      ELLE_TRACE_METHOD(network_id, path);

      if (elle::os::path::exists(path))
        return;

      using elle::serialize::from_string;
      using elle::serialize::InputBase64Archive;

      auto network = this->one(network_id);

      elle::io::Path shelter_path(path);
      nucleus::proton::Network proton_network(network_id);
      hole::storage::Directory storage(proton_network, shelter_path.string());

      {
        ELLE_DEBUG("Built directory storage of %s", network_id);

        ELLE_DEBUG("root block %s", network.root_block);
        ELLE_DEBUG("descriptor %s", network.descriptor);

        ELLE_ASSERT(network.root_block.size());
        ELLE_ASSERT(network.descriptor.size());

        ELLE_DEBUG("Create lune descriptor of %s", network_id);

        lune::Descriptor descriptor{
          from_string<InputBase64Archive>(network.descriptor)
        };
        ELLE_DEBUG("Lune descriptor created");

        // XXX[pas forcement necessaire si le format n'a pas change entre
        //     la version du descriptor et celle d'Infinit. il faudrait
        //     comparer static format avec ceux de reference dans le descriptor]
        //if (descriptor.version() > Infinit::version)
        //{
        //  throw elle::Exception("you have to update Infinit");
        //}

        // XXX[ici plutot compare static/dynamic format car on peut passer
        //     a une nouvelle version sans que le descriptor n'ait change
        //     de format]
        //if (description.version() < Infinit::version && je suis owner)
        //{
        //  static_assert(false, "migrate the descriptor here & send to meta");
        //}

        lune::Identity identity;
        identity.Restore(this->_meta.identity());

        ELLE_DEBUG("Storing the descriptor of %s for user %s",
                   network_id, this->_self().id);
        descriptor.store(identity);

        nucleus::neutron::Object directory{
          from_string<InputBase64Archive>(network.root_block)
        };

        storage.store(descriptor.meta().root(), directory);
        ELLE_DEBUG("Root block stored.");
      }

      {
        ELLE_DEBUG("Storing group block.");
        ELLE_DEBUG("block: '%s'.", network.group_block);
        nucleus::neutron::Group group{
          from_string<InputBase64Archive>(network.group_block)
        };
        ELLE_DEBUG("address: '%s'.", network.group_address);
        nucleus::proton::Address group_address{
          from_string<InputBase64Archive>(network.group_address)
        };
        ELLE_DEBUG("Deserialization complete.");
        storage.store(group_address, group);
        ELLE_DEBUG("Group block stored.");
      }

      ELLE_DEBUG("End of prepare directory");
    }

    std::string
    NetworkManager::delete_(std::string const& network_id,
                            bool remove_directory)
    {
      ELLE_TRACE_METHOD(network_id, remove_directory);

      ELLE_SCOPE_EXIT([&] {
        if (remove_directory)
          this->delete_local(network_id);
      });

      if (this->infinit_instance_manager().exists(network_id))
      {
        ELLE_TRACE("stoping infinit instance for network %s", network_id)
          this->_infinit_instance_manager.stop(network_id);
      }

      if (this->_networks->find(network_id) != this->_networks->end())
      {
        this->_reporter.store("network_delete_attempt",
                              {{MKey::value,  network_id}});
        ELLE_TRACE("remove network %s from meta", network_id)
          try
          {
            this->_meta.delete_network(network_id, true);
          }
          CATCH_FAILURE_TO_METRICS("network_delete");
        this->_reporter.store("network_delete_succeed",
                              {{MKey::value, network_id}});
      }

      if (this->_networks->find(network_id) != this->_networks->end())
        this->_networks->erase(network_id);

      return network_id;
    }

    void
    NetworkManager::delete_local(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);

      auto path = common::infinit::network_directory(this->_self().id,
                                                     network_id);
      if (elle::os::path::exists(path))
      {
        ELLE_TRACE("remove network %s directory %s", network_id, path)
          elle::os::path::remove_directory(path);
      }
    }

    std::vector<std::string>
    NetworkManager::all_ids()
    {
      ELLE_TRACE_METHOD("");

      return this->_networks([](NetworkMap const& map) {
          std::vector<std::string> res{map.size()};
          for (auto const& pair: map)
            res.emplace_back(pair.first);
          return res;
      });
    }

    Network
    NetworkManager::one(std::string const& id)
    {
      ELLE_TRACE_METHOD("");

      if (this->_networks->find(id) == this->_networks->end())
        return this->sync(id);
      return this->_networks[id];
    }

    Network
    NetworkManager::sync(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

        try
        {
          auto network = this->_meta.network(id);
          return this->_networks(
            [&id, &network] (NetworkMap& map) -> Network
            {
              return (map[id] = network);
            });
        }
        catch (elle::Exception const& e)
        {
          throw Exception{gap_network_error, e.what()};
        }
      elle::unreachable();
    }

    void
    NetworkManager::add_user(std::string const& network_id,
                             std::string const& peer_public_key)
    {
      ELLE_TRACE_METHOD(network_id, peer_public_key);

      this->_reporter.store("network_adduser_attempt",
                            {{MKey::value, network_id}});

      try
      {
        nucleus::neutron::User::Identity public_key;
        public_key.Restore(peer_public_key);

        nucleus::neutron::Subject subject;
        subject.Create(public_key);

        auto const& network = this->one(network_id);

        nucleus::neutron::Group::Identity group;
        group.Restore(network.group_address);

        this->_infinit_instance_manager.add_user(network._id,
                                                 group,
                                                 subject);
      }
      CATCH_FAILURE_TO_METRICS("network_adduser");

      this->_reporter.store("network_adduser_succeed",
                            {{MKey::value, network_id}});
    }

    void
    NetworkManager::add_device(std::string const& network_id,
                               std::string const& device_id)
    {
      ELLE_TRACE_METHOD(network_id, device_id);

      this->_meta.network_add_device(network_id, device_id);
      this->sync(network_id);
    }

    void
    NetworkManager::_on_network_update(NetworkUpdateNotification const& notif)
    {
      ELLE_TRACE_METHOD(notif);

      // XXX do something
    }

    void
    NetworkManager::set_permissions(std::string const& network_id,
                                    std::string const& peer_public_key)
    {
      ELLE_TRACE_SCOPE("%s: set permission on '/' for user %s on network %s",
                       *this, peer_public_key, network_id);

      nucleus::neutron::User::Identity public_key;
      public_key.Restore(peer_public_key);

      nucleus::neutron::Subject subject;
      subject.Create(public_key);

      this->_infinit_instance_manager.grant_permissions(network_id, subject);
    }

    void
    NetworkManager::upload_files(std::string const& network_id,
                                 std::unordered_set<std::string> const& files,
                                 std::function<void ()> success_callback,
                                 std::function<void ()> failure_callback)

    {
      ELLE_TRACE_SCOPE("%s: uploading %s into network %s",
                       *this, files, network_id);
      this->_infinit_instance_manager.upload_files(network_id,
                                                   files,
                                                   success_callback,
                                                   failure_callback);
    }

    void
    NetworkManager::download_files(std::string const& network_id,
                                   std::vector<std::string> const& addresses,
                                   std::string const& public_key,
                                   std::string const& destination,
                                   std::function<void ()> success_callback,
                                   std::function<void ()> failure_callback)
    {
      ELLE_TRACE_SCOPE("%s: uploading files into network %s",
                       *this, network_id);

      nucleus::neutron::User::Identity _public_key;
      _public_key.Restore(public_key);

      nucleus::neutron::Subject subject;
      subject.Create(_public_key);

      this->_infinit_instance_manager.download_files(network_id,
                                                     addresses,
                                                     subject,
                                                     destination,
                                                     success_callback,
                                                     failure_callback);
    }

    float
    NetworkManager::progress(std::string const& network_id)
    {
      ELLE_TRACE_SCOPE("%s: getting progress for %s",
                       *this, network_id);

      return this->_infinit_instance_manager.progress(network_id);
    }

    static
    inline
    std::vector<std::string>
    _find_commond_addr(std::vector<std::string> const &externals,
                       std::vector<std::string> const &my_externals)
    {
      ELLE_DEBUG_FUNCTION(externals, my_externals);

      std::vector<std::string> theirs_addr;
      std::vector<std::string> ours_addr;
      std::vector<std::string> common_addr;

      // XXX[refactor this]
      for (auto const& i: externals)
      {
        std::vector<std::string> res;
        boost::split(res, i, boost::is_any_of(":"));
        theirs_addr.push_back(res[0]);
      }
      // XXX[refactor this]
      for (auto const& i: my_externals)
      {
        std::vector<std::string> res;
        boost::split(res, i, boost::is_any_of(":"));
        ours_addr.push_back(res[0]);
      }

      std::set_intersection(begin(theirs_addr), end(theirs_addr),
                            begin(ours_addr), end(ours_addr),
                            std::back_inserter(common_addr));
      return common_addr;
    }

    std::vector<std::string>
    NetworkManager::peer_addresses(std::string const& network_id,
                                    std::string const& sender_device_id,
                                    std::string const& recipient_device_id)
    {
      ELLE_DEBUG_SCOPE("peer_addresses net(%s), send(%s), rec(%s)",
                       network_id, sender_device_id, recipient_device_id);

      ELLE_ASSERT(this->_device().id == sender_device_id ||
                  this->_device().id == recipient_device_id);

      bool const sender = recipient_device_id != this->_device().id;
      ELLE_DEBUG_SCOPE("action as the %s", sender ? "sender" : "recipient");

      namespace proto = infinit::protocol;

      /// Check if network is valid
      {
        if (this->_networks->find(network_id) == this->_networks->end())
          throw gap::Exception{gap_internal_error, "Unable to find network"};
      }

      // Fetch Nodes and find the correct one to contact
      std::vector<std::string> externals;
      std::vector<std::string> locals;
      std::vector<std::string> my_externals;
      std::vector<std::string> my_locals;
      std::vector<std::string> fallback;
      {
        std::string theirs_device;
        std::string ours_device;

        if (!sender)
        {
          theirs_device = sender_device_id;
          ours_device = recipient_device_id;
        }
        else
        {
          theirs_device = recipient_device_id;
          ours_device = sender_device_id;
        }

        // theirs
        {
          Endpoint e = this->_meta.device_endpoints(network_id,
                                                    ours_device,
                                                    theirs_device);

          externals = std::move(e.externals);
          locals = std::move(e.locals);
        }
        //ours
        {
          Endpoint e = this->_meta.device_endpoints(network_id,
                                                    theirs_device,
                                                    ours_device);

          my_externals = std::move(e.externals);
          my_locals = std::move(e.locals);
          fallback = std::move(e.fallback);
        }
      }

      static auto _print = [] (std::string const &s) { ELLE_DEBUG("-- %s", s); };

      ELLE_DEBUG("externals")
        std::for_each(begin(externals), end(externals), _print);
      ELLE_DEBUG("locals")
        std::for_each(begin(locals), end(locals), _print);
      ELLE_DEBUG("fallback")
        std::for_each(begin(fallback), end(fallback), _print);

      std::vector<std::string> addresses;
      addresses.resize(fallback.size()); // + locals.size() + externals.size());

      auto it = addresses.begin();
      for (auto const& round: {fallback}) //, locals, externals})
        it = std::copy(round.begin(), round.end(), it);

      for (auto const& addr: addresses)
        ELLE_TRACE("destination address selected: %s", addr);
      return addresses;
    }

    void
    NetworkManager::launch(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);

      // XXX is this normal? at this point the network has
      //     obviously been created and prepared!
      this->prepare(network_id);

      // XXX: do not restore the identity every time.
      lune::Identity identity;
      if (identity.Restore(this->_meta.identity()) == elle::Status::Error)
        throw elle::Exception("Couldn't restore the identity.");

      this->_infinit_instance_manager.launch(
        network_id,
        identity,
        this->one(network_id).descriptor);
    }

    /*----------.
    | Printable |
    `----------*/
    void
    NetworkManager::print(std::ostream& stream) const
    {
      stream << "NetworkManager(" << this->_meta.email() << ")";
    }
  }
}
