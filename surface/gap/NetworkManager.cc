#include <surface/gap/NetworkManager.hh>
#include <surface/gap/metrics.hh>
// XXX: Only requiered by gep_exec...
#include <surface/gap/gap.h>

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
#include <elle/io/Piece.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/system/Process.hh>


#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>


ELLE_LOG_COMPONENT("infinit.surface.gap.Network");

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
      throw std::runtime_error("Couldn't restore the identity.");

    //- group ------------------------------------------------------------------
    nucleus::neutron::Group group(network, identity.pair().K(), "everybody");
    group.seal(identity.pair().k());

    //- group address ----------------------------------------------------------
    nucleus::proton::Address  group_address(group.bind());

    //- subject ----------------------------------------------------------------
    nucleus::neutron::Subject subject;
    if (subject.Create(group_address) == elle::Status::Error)
      throw std::runtime_error("unable to create the group subject");

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
      throw std::runtime_error("unable to update the directory");

    if (directory.Seal(identity.pair().k(), access_fingerprint) == e)
      throw std::runtime_error("Cannot seal the access");

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
                                   Self const& me,
                                   Device const& device):
      _meta(meta),
      _reporter(reporter),
      _google_reporter(google_reporter),
      _self(me),
      _device(device),
      _infinit_instance_manager{me.id}
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
      // XXX.
      this->_infinit_instance_manager.clear();
    }

    std::string
    NetworkManager::create(std::string const& name,
                           bool auto_add)
    {
      ELLE_TRACE("creating network %s", name);

      this->_reporter.store("network_create", {{MKey::status, "attempt"}});

      this->_google_reporter.store("network:create:attempt");

      plasma::meta::CreateNetworkResponse response;
      try
      {
        response = this->_meta.create_network(name);
      }
      CATCH_FAILURE_TO_METRICS("network_create");

      this->_reporter.store("network_create",
                            {{MKey::status, "succeed"},
                             {MKey::value, response.created_network_id}});

      this->_google_reporter.store("network:create:succeed");

      // XXX: Device manager.
      if (auto_add)
        this->_meta.network_add_device(response.created_network_id,
                                       this->_device.id);

      // Fetch network.
      this->one(response.created_network_id);

      return response.created_network_id;
    }

    void
    NetworkManager::prepare(std::string const& network_id)
    {
      ELLE_TRACE("preparing network %s directory", network_id);
      std::string const network_dir = common::infinit::network_directory(
        this->_self.id,
        network_id);

      ELLE_DEBUG("network directory: %s", network_dir);
      if (!elle::os::path::exists(network_dir))
        elle::os::path::make_path(network_dir);

      std::string const description_filename =
        common::infinit::descriptor_path(this->_self.id, network_id);

      ELLE_DEBUG("descriptor path: %s", description_filename);

      if (!elle::os::path::exists(description_filename))
      {
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

      auto& network = this->one(network_id);

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
        //  throw std::runtime_error("you have to update Infinit");
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
                   network_id, this->_self.id);
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
      ELLE_TRACE_METHOD(network_id);
      this->_all()([&network_id] (NetworkMapPtr& map) {
          map->at(network_id).reset();
      });

      this->_infinit_instance_manager.stop(network_id);

      this->_reporter.store("network_delete",
                            {{MKey::status, "attempt"},
                             {MKey::value,  network_id}});

      plasma::meta::DeleteNetworkResponse response;
      try
      {
        response = this->_meta.delete_network(network_id, force);
      }
      CATCH_FAILURE_TO_METRICS("network_delete");

      this->_reporter.store("network_delete",
                            {{MKey::status, "succeed"},
                             {MKey::value,  response.deleted_network_id}});

      if (this->infinit_instance_manager().exists(response.deleted_network_id))
      {
        this->_infinit_instance_manager.stop(response.deleted_network_id);
      }

      std::string network_path =
        common::infinit::network_directory(this->_self.id, network_id);

      if (elle::os::path::exists(network_path))
        elle::os::path::remove_directory(network_path);

      return response.deleted_network_id;
    }

    NetworkManager::NetworkMapMonitor&
    NetworkManager::_all()
    {
      if (this->_networks->get() != nullptr)
        return this->_networks;

      this->_networks->reset(new NetworkManager::NetworkMap{});

      auto response = this->_meta.networks();
      for (auto const& id: response.networks)
      {
        auto network = this->_meta.network(id);
        this->_networks([&id, &network] (NetworkMapPtr& map) {
          (*map)[id].reset(new Network{network});
        });
      }

      return this->_networks;
    }

    std::vector<std::string>
    NetworkManager::all_ids()
    {
      return this->_all()([](NetworkMapPtr const& map) {
          std::vector<std::string> res{map->size()};
          for (auto const& pair: *map)
            res.emplace_back(pair.first);
          return res;
      });
    }

    Network const&
    NetworkManager::one(std::string const& id)
    {
      auto ptr = this->_all()([&id] (NetworkMapPtr& map) -> Network const*{
        auto it = map->find(id);
        if (it != map->end())
        {
          if (it->second == nullptr)
            throw Exception("getting a deleted network");
          return it->second.get();
        }
        return nullptr;
      });
      if (ptr != nullptr)
        return *ptr;
      return this->sync(id);
    }

    Network const&
    NetworkManager::sync(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);
      this->_all()([&id] (NetworkMapPtr& map) {
        auto it = map->find(id);
        if (it != map->end())
        {
          if (it->second == nullptr)
            throw Exception("Sync a delete nework");
        }
      });

      try
      {
        auto network = this->_meta.network(id);
        ELLE_DEBUG("Synched %s", id);
        return this->_all()([&id, &network] (NetworkMapPtr& map) -> Network const& {
          (*map)[id].reset(new Network{network});
          return *(map->at(id));
        });
      }
      catch (std::runtime_error const& e)
      {
        throw Exception{gap_network_error, e.what()};
      }
    }

    void
    NetworkManager::add_user(std::string const& network_id,
                             std::string const& owner,
                             std::string const& user_id,
                             std::string const& user_identity)
    {
      ELLE_TRACE_METHOD(network_id, user_id);

      this->_reporter.store("network_adduser",
                            {{MKey::status, "attempt"},
                             {MKey::value, network_id}});

      try
      {
        Network const& network = this->one(network_id);

        ELLE_DEBUG("locating 8 group");
        std::string const& group_binary =
          common::infinit::binary_path("8group");

        std::list<std::string> arguments{
          "--user", owner,
          "--type", "user",
          "--add",
          "--network", network._id,
          "--identity", user_identity
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

        auto res = this->_meta.network_add_user(network_id, user_id);
        this->sync(network_id);
      }
      CATCH_FAILURE_TO_METRICS("network_adduser");

      this->_reporter.store("network_adduser",
                            {{MKey::status, "succeed"},
                              {MKey::value, network_id}});
    }

    void
    NetworkManager::add_device(std::string const& network_id,
                               std::string const& device_id)
    {
      this->_meta.network_add_device(network_id, device_id);
      this->sync(network_id);
    }

    void
    NetworkManager::_on_network_update(NetworkUpdateNotification const& notif)
    {
      ELLE_TRACE("network %s updated %s", notif.network_id, notif.what);
      // XXX do something
    }

    void
    NetworkManager::set_permissions(std::string const& network_id,
                                    std::string const& user_id,
                                    std::string const& user_identity,
                                    nucleus::neutron::Permissions permissions)
    {
      ELLE_TRACE("setting permissions");

      // TODO: Do this only on the current device for sender and recipient.
      this->wait_portal(network_id);

      std::string const& access_binary =
        common::infinit::binary_path("8access");

      std::list<std::string> arguments{
        "--user", this->_self.id,
        "--type", "user",
        "--grant",
        "--network", network_id,
        "--path", "/",
        "--identity", user_identity,
      };

      if (permissions & nucleus::neutron::permissions::read)
        arguments.push_back("--read");
      if (permissions & nucleus::neutron::permissions::write)
        arguments.push_back("--write");

      ELLE_DEBUG("LAUNCH: %s %s",
                 access_binary,
                 boost::algorithm::join(arguments, " "));

      if (permissions & gap_exec)
      {
        ELLE_WARN("XXX: setting executable permissions not yet implemented");
      }

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
          log_file += ".access.log";
          pc.setenv("ELLE_LOG_FILE", log_file);
        }
      }
      elle::system::Process p{std::move(pc), access_binary, arguments};
      if (p.wait_status() != 0)
        throw Exception(gap_internal_error, "8access binary failed");
    }

    static
    int
    _connect_try(reactor::Scheduler& sched,
                 hole::implementations::slug::control::RPC& rpcs,
                 std::vector<std::string> const& addresses)
    {
      typedef std::unique_ptr<reactor::VThread<bool>> VThreadBoolPtr;
      std::vector<std::pair<VThreadBoolPtr, std::string>> v;

      auto slug_connect = [&] (std::string const& endpoint)
        {
          std::vector<std::string> result;
          boost::split(result, endpoint, boost::is_any_of(":"));

          auto const &ip = result[0];
          auto const &port = result[1];
          ELLE_DEBUG("slug_connect(%s, %s)", ip, port)
          rpcs.slug_connect(ip, std::stoi(port));

          ELLE_DEBUG("slug_wait(%s, %s)", ip, port)
          if (!rpcs.slug_wait(ip, std::stoi(port)))
            throw elle::Exception(elle::sprintf("slug_wait(%s, %s) failed",
                                                ip, port));
        };

      auto start_thread = [&] (std::string const &endpoint)
        {
          v.push_back(std::make_pair(
                        elle::make_unique<reactor::VThread<bool>>(
                          sched,
                          elle::sprintf("slug_connect(%s)", endpoint),
                          [&] () -> int
                          {
                            try
                            {
                              slug_connect(endpoint);
                            }
                            catch (elle::Exception const &e)
                            {
                              ELLE_WARN("slug_connect failed: %s", e.what());
                              return false;
                            }
                            return true;
                          }),
                        endpoint));
        };

      ELLE_DEBUG("Connecting...")
        std::for_each(std::begin(addresses), std::end(addresses), start_thread);

      int i = 0;
      for (auto &t : v)
      {
        reactor::VThread<bool> &vt = *t.first;
        sched.current()->wait(vt);
        if (vt.result() == true)
        {
          i++;
          ELLE_WARN("connection to %s succeed", t.second);
        }
        else
        {
          ELLE_WARN("connection to %s failed", t.second);
        }
      }
      ELLE_TRACE("finish connecting to %d node%s", i, i > 1 ? "s" : "");
      return i;
    }

    static
    std::vector<std::string>
    _find_commond_addr(std::vector<std::string> const &externals,
                       std::vector<std::string> const &my_externals)
    {
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


    auto _print = [] (std::string const &s) { ELLE_DEBUG("-- %s", s); };

    void
    NetworkManager::notify_8infinit(std::string const& network_id,
                                    std::string const& sender_device_id,
                                    std::string const& recipient_device_id)
    {
      std::exception_ptr exception;
      {
        reactor::Scheduler sched;
        reactor::Thread sync{sched, "notify_8infinit", [&] {
            try
            {
              this->_notify_func();
            }
            // A parsing bug in gcc (fixed in 4.8.3) make this block
            // mandatory.
            catch (std::exception const&)
            {
              exception = std::current_exception();
            }
            catch (...)
            {
              exception = std::current_exception();
            }
          }
        };

        sched.run();
        ELLE_DEBUG("notify finished");
      }
      if (exception != std::exception_ptr{})
      {
        ELLE_ERR("cannot connect infinit instances: %s",
                 elle::exception_string(exception));
        std::rethrow_exception(exception);
      }
    }

    void
    NetworkManager::_notify_8infinit(std::string const& network_id,
                                     std::string const& sender_device_id,
                                     std::string const& recipient_device_id,
                                     reactor::Scheduler& sched)
    {
      ELLE_TRACE_METHOD(network_id, sender_device_id, recipient_device_id);
      ELLE_ASSERT(this->_device.id == sender_device_id ||
                  this->_device.id == recipient_device_id);

      namespace proto = infinit::protocol;

      /// Check if network is valid
      {
        auto network = this->_all()->get()->find(network_id);

        if (network == this->_all()->get()->end())
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

        if (recipient_device_id == this->_device.id)
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

      ELLE_DEBUG("externals")
        std::for_each(begin(externals), end(externals), _print);
      ELLE_DEBUG("locals")
        std::for_each(begin(locals), end(locals), _print);
      ELLE_DEBUG("fallback")
        std::for_each(begin(fallback), end(fallback), _print);

      // Very sophisticated heuristic to deduce the addresses to try first.
      std::vector<std::vector<std::string>> rounds;
      {
        std::vector<std::string> common = _find_commond_addr(externals,
                                                             my_externals);
        std::vector<std::string> first_round;
        std::vector<std::string> second_round;

        // sort the list, in order to have a deterministic behavior
        std::sort(begin(externals), end(externals));
        std::sort(begin(locals), end(locals));
        std::sort(begin(fallback), end(fallback));

        if (externals.empty() || my_externals.empty())
        {
          for (auto const& s: locals)
            first_round.push_back(s);
          rounds.push_back(first_round);
          for (auto const& s: fallback)
            second_round.push_back(s);
          rounds.push_back(second_round);
        }
        else if (common.empty())
        {
          // if there is no common external address, then we can try them first.
          for (auto const& s: externals)
            first_round.push_back(s);
          rounds.push_back(first_round);
          // then, we know we can not connect locally, so try to fallback
          for (auto const& s: fallback)
            second_round.push_back(s);
          rounds.push_back(second_round);
        }
        else
        {
          // if there is a common external address, we can try to connect to
          // local endpoints
          std::vector<std::string> addr = _find_commond_addr(locals,
                                                             my_locals);

          if (!addr.empty())
          {
            // wtf, you are trying to do a local exchange, this is stupid, but
            // let it be.
            first_round.push_back(locals.front());
            rounds.push_back(first_round);
            if (addr.size() > 1)
            {
              second_round.push_back(locals.back());
              rounds.push_back(second_round);
            }
          }
          else
          {
            rounds.push_back(locals);
            rounds.push_back(externals);
            rounds.push_back(fallback);
          }
        }
      }

      // Finish by calling the RPC to notify 8infinit of all the IPs of the peer
      {
        lune::Phrase phrase;

        this->wait_portal(network_id);

        phrase.load(this->_self.id, network_id, "slug");

        ELLE_DEBUG("Connect to the local 8infint instance (%s:%d)",
                   elle::String{"127.0.0.1"},
                   phrase.port);

        // Connect to the server.
        reactor::network::TCPSocket socket{
          sched,
          elle::String("127.0.0.1"),
          phrase.port,
        };

        proto::Serializer serializer{sched, socket};
        proto::ChanneledStream channels{sched, serializer};
        hole::implementations::slug::control::RPC rpcs{channels};

        int i = 1;
        ELLE_DEBUG("DEBUG ROUNDS")
        {
          for (auto const& round: rounds)
          {
            ELLE_DEBUG("- ROUND %s", i++)
            {
              for (auto const& addr: round)
              {
                ELLE_DEBUG("-- %s", addr);
              }
            }
          }
        }

        int round_number = 1;
        bool success = false;
        for (auto const& round: rounds)
        {
          ELLE_DEBUG("ROUND %s:", round_number++)
          {
            for (auto const&s : round)
              ELLE_DEBUG("-- %s", s);
          }
          if (_connect_try(sched, rpcs, round) > 0)
          {
            success = true;
            break;
          }
        }
        if (!success)
          throw elle::Exception{"Unable to connect"};
      }
    }

    void
    NetworkManager::wait_portal(std::string const& network_id)
    {
      ELLE_TRACE_METHOD(network_id);
      this->prepare(network_id);
      this->_infinit_instance_manager.wait_portal(network_id);
    }
  }
}
