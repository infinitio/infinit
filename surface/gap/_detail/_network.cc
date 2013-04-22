#include <surface/gap/State.hh>

#include "../NetworkManager.hh"

#include <boost/algorithm/string/join.hpp>

#include <common/common.hh>

#include <lune/Descriptor.hh>
#include <lune/Identity.hh>
#include <lune/Set.hh>
#include <lune/Lune.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Porcupine.hh>
#include <nucleus/neutron/Genre.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Trait.hh>
#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Access.hh>

#include <hole/storage/Directory.hh>
#include <hole/Passport.hh>

#include <metrics/Reporter.hh>

#include <elle/format/json.hh>
#include <elle/io/Piece.hh>
#include <elle/serialize/insert.hh>
#include <elle/serialize/extract.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/system/Process.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

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
    ELLE_DEBUG("Creating the network descriptor for network %s.", id);
    // XXX this value depends on the network policy and openness.
    static nucleus::neutron::Permissions permissions =
      nucleus::neutron::permissions::read;

    auto e = elle::Status::Error;
    auto genreDirectory = nucleus::neutron::Genre::directory;

    ELLE_DEBUG("Create proton network from id '%s'.", id);
    nucleus::proton::Network network(id);

    //- identity ----------------------------------------------------------------
    lune::Identity        identity;
    if (identity.Restore(identity_) == e)
      throw std::runtime_error("Couldn't restore the identity.");

    //- group -------------------------------------------------------------------
    nucleus::neutron::Group group(network, identity.pair().K(), "everybody");
    group.seal(identity.pair().k());

    //- group address -----------------------------------------------------------
    nucleus::proton::Address  group_address(group.bind());

    //- subject -----------------------------------------------------------------
    nucleus::neutron::Subject subject;
    if (subject.Create(group_address) == elle::Status::Error)
      throw std::runtime_error("unable to create the group subject");

    //- access-------------------------------------------------------------------
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
    static cryptography::SecretKey secret_key(cryptography::cipher::Algorithm::aes256,
                                              ACCESS_SECRET_KEY);

    ELLE_ASSERT(access_porcupine.strategy() == nucleus::proton::Strategy::value);

    cryptography::Digest access_fingerprint =
      nucleus::neutron::access::fingerprint(access_porcupine);

    nucleus::proton::Radix access_radix = access_porcupine.seal(secret_key);

    //- directory ---------------------------------------------------------------
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
      throw std::runtime_error("unable to update the directory");

    if (directory.Seal(identity.pair().k(), access_fingerprint) == e)
      throw std::runtime_error("Cannot seal the access");

    //- directory address -------------------------------------------------------
    nucleus::proton::Address  directory_address(directory.bind());

    NetworkBlocks nb;
    elle::io::Unique root_block_;
    elle::serialize::to_string<elle::serialize::OutputBase64Archive>(
      nb.root_block) << directory;

    elle::io::Unique root_address_;
    elle::serialize::to_string<elle::serialize::OutputBase64Archive>(
      nb.root_address) << directory_address;

    elle::io::Unique group_block_;
    elle::serialize::to_string<elle::serialize::OutputBase64Archive>(
      nb.group_block) << group;

    elle::io::Unique group_address_;
    elle::serialize::to_string<elle::serialize::OutputBase64Archive>(
      nb.group_address) << group_address;

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

    std::string
    NetworkManager::create(std::string const& name,
                           bool auto_add)
    {
      ELLE_TRACE("creating network %s", name);

      reporter().store("network_create", {{MKey::status, "attempt"}});

      google_reporter().store("network:create:attempt");

      plasma::meta::CreateNetworkResponse response;
      try
      {
        response = this->_state._meta.create_network(name);
      }
      CATCH_FAILURE_TO_METRICS("network_create");

      reporter().store("network_create",
                       {{MKey::status, "succeed"},
                        {MKey::value, response.created_network_id}});

      google_reporter().store("network:create:succeed");

      this->networks_dirty = true; // XXX insert response in _networks

      if (auto_add)
        this->_state._meta.network_add_device(response.created_network_id,
                                               this->_state.device_id());
      return response.created_network_id;
    }

    void
    NetworkManager::prepare(std::string const& network_id)
    {
      ELLE_TRACE("preparing network %s directory", network_id);
      std::string const network_dir = common::infinit::network_directory(
        this->_state.me()._id,
        network_id);

      ELLE_DEBUG("network directory: %s", network_dir);
      if (!elle::os::path::exists(network_dir))
        elle::os::path::make_path(network_dir);

      std::string const description_filename = elle::os::path::join(
        network_dir,
        network_id + ".dsc"
      );

      ELLE_DEBUG("descriptor path: %s", description_filename);

      if (!elle::os::path::exists(description_filename))
      {
        if (!this->one(network_id).descriptor.size())
        {
          auto nb = create_network_root_block(network_id,
                                              this->_state._meta.identity());

          this->_state._meta.update_network(network_id,
                                                    nullptr,
                                                    &nb.root_block,
                                                    &nb.root_address,
                                                    &nb.group_block,
                                                    &nb.group_address);

          // XXX: remove networks_dirty asap.
          this->networks_dirty = true;  //  XXX set root_block
        }
        this->_prepare_directory(network_id);
        this->_state._wait_portal(network_id);
      }
      else
      {
        ELLE_DEBUG("network %s already prepared", network_id);
      }
    }

    void
    NetworkManager::_prepare_directory(std::string const& network_id)
    {
      ELLE_DEBUG("Prepare network %s directory.", network_id);
      using elle::serialize::from_string;
      using elle::serialize::InputBase64Archive;

      auto& network = this->one(network_id);

      elle::io::Path shelter_path(lune::Lune::Shelter);
      shelter_path.Complete(
        elle::io::Piece{"%USER%", this->_state.me()._id},
        elle::io::Piece{"%NETWORK%", network_id}
      );
      ELLE_DEBUG("Shelter path == %s", shelter_path.string());
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
      //  {
      //    throw std::runtime_error("you have to update Infinit");
      //  }

      // XXX[ici plutot compare static/dynamic format car on peut passer
      //     a une nouvelle version sans que le descriptor n'ait change
      //     de format]
      //if (description.version() < Infinit::version && je suis owner)
      //  {
      //     static_assert(false, "migrate the descriptor here and send to meta");
      //  }

        lune::Identity identity;
        identity.Restore(this->_state._meta.identity());

        ELLE_DEBUG("Storing the descriptor of %s for user %s",
            network_id, this->_state.me()._id);
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
    NetworkManager::delete_(std::string const& network_id, bool force)
    {
      ELLE_TRACE("deleting network %s", network_id);

      this->_networks[network_id].reset();

      reporter().store("network_delete",
                            {{MKey::status, "attempt"},
                             {MKey::value,  network_id}});

      plasma::meta::DeleteNetworkResponse response;
      try
      {
        response = this->_state._meta.delete_network(network_id, force);
      }
      CATCH_FAILURE_TO_METRICS("network_delete");

      reporter().store("network_delete",
                            {{MKey::status, "succeed"},
                             {MKey::value,  response.deleted_network_id}});


      this->networks_dirty = true; // XXX remove from _networks instead
      if (this->_state.infinit_instance_manager().has_network(response.deleted_network_id))
      {
        this->_state.infinit_instance_manager().stop_network(
          response.deleted_network_id
          );
      }

      std::string network_path = common::infinit::network_directory(
        this->_state.me()._id, network_id);

      if (elle::os::path::exists(network_path))
        elle::os::path::remove_directory(network_path);

      return response.deleted_network_id;
    }

    std::map<std::string, NetworkManager::NetworkPtr> const&
    NetworkManager::all()
    {
      if (this->networks_dirty)
        {
          auto response = this->_state._meta.networks();
          for (auto const& network_id: response.networks)
            {
              auto response = this->_state._meta.network(network_id);
              this->_networks[network_id].reset(new Network{response});
            }
          this->networks_dirty = false;
        }
      return this->_networks;
    }

    Network&
    NetworkManager::one(std::string const& id)
    {
      auto it = this->all().find(id);
      if (it == this->all().end() || it->second == nullptr)
        {
          try
            {
              auto response = this->_state._meta.network(id);
              this->_networks[id].reset(new Network{response});
              return *(this->_networks[id]);
            }
          catch (std::runtime_error const& e)
            {
              throw Exception{gap_network_not_found, e.what()};
            }
        }
      return *(it->second);
    }

    void
    NetworkManager::add_user(std::string const& network_id,
                             std::string const& owner,
                             std::string const& user_id)
    {
      ELLE_TRACE_FUNCTION(network_id, user_id);

      reporter().store("network_adduser",
                       {{MKey::status, "attempt"},
                        {MKey::value, network_id}});

      try
      {
        Network& network = this->one(network_id);

        ELLE_DEBUG("locating 8 group");
        std::string const& group_binary = common::infinit::binary_path("8group");

        std::list<std::string> arguments{
                                    "--user",
                                    owner,
                                    "--type",
                                    "user",
                                    "--add",
                                    "--network",
                                    network._id,
                                    "--identity",
                                    this->_state.user(user_id).public_key
                                };

        ELLE_DEBUG("LAUNCH: %s %s",
                   group_binary,
                   boost::algorithm::join(arguments, " "));
        auto pc = elle::system::process_config(elle::system::normal_config);
        {
          std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

          if (!log_file.empty())
          {
            if (elle::os::in_env("INFINIT_LOG_FILE_PID"))
            {
              log_file += ".";
              log_file += std::to_string(::getpid());
            }
            log_file += ".group.log";
            pc.setenv("ELLE_LOG_FILE", log_file);
          }
        }
        elle::system::Process p{std::move(pc), group_binary, arguments};
        if (p.wait_status() != 0)
          throw Exception(gap_internal_error, "8group binary failed");

        ELLE_DEBUG("set user in network in meta.");

        auto res = this->_state._meta.network_add_user(network_id, user_id);
        if (std::find(network.users.begin(),
                      network.users.end(),
                      user_id) == network.users.end())
          network.users.push_back(user_id);
      }
      CATCH_FAILURE_TO_METRICS("network_adduser");

      reporter().store("network_adduser",
                            {{MKey::status, "succeed"},
                             {MKey::value, network_id}});
    }

    void
    NetworkManager::_on_network_update(NetworkUpdateNotification const& notif)
    {
      ELLE_TRACE("network %s updated %s", notif.network_id, notif.what);
      // XXX do something
    }

  }
}
