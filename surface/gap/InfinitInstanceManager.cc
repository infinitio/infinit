#include <signal.h>
#include <stdlib.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/logic/tribool.hpp>

#include <HoleFactory.hh>

# include <reactor/duration.hh>

#include <elle/Exception.hh>
#include <elle/network/Interface.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>
#include <elle/system/signal.hh>

#include <etoile/Etoile.hh>
#include <etoile/wall/Access.hh>
#include <etoile/wall/Group.hh>
#include <etoile/wall/Object.hh>
#include <etoile/wall/Path.hh>
#include <etoile/path/Chemin.hh>

#include <common/common.hh>
#include <surface/gap/_detail/TransferOperations.hh>
#include <surface/gap/Exception.hh>
#include <surface/gap/InfinitInstanceManager.hh>
#include <surface/gap/binary_config.hh>

#include <plasma/meta/Client.hh>

#include <reactor/sleep.hh>


ELLE_LOG_COMPONENT("infinit.surface.gap.InfinitInstanceManager");

namespace surface
{
  namespace gap
  {
    InfinitInstance::InfinitInstance(std::string const& user_id,
                                     std::string const& meta_host,
                                     uint16_t meta_port,
                                     std::string const& token,
                                     std::string const& network_id,
                                     lune::Identity const& identity,
                                     std::string const& descriptor):
      network_id(network_id),
      mount_point(),
      network(network_id),
      identity(identity),
      passport(),
      descriptor(
        elle::serialize::from_string<elle::serialize::InputBase64Archive>(
          descriptor)),
      storage(this->network, common::infinit::network_shelter(user_id,
                                                              network_id)),
      hole(),
      etoile(),
      scheduler(),
      keep_alive(scheduler, "Keep alive", [] ()
                 {
                   while (true)
                   {
                     auto* current = reactor::Scheduler::scheduler()->current();
                     current->sleep(boost::posix_time::seconds(60));
                   }
                 }),
      thread(std::bind(&reactor::Scheduler::run, std::ref(scheduler))),
      progress{0.0f},
      progress_mutex{},
      progress_thread{}
    {
      this->scheduler.mt_run<void>(
        elle::sprintf("initializer for %s", network_id),
        [&]
        {
          elle::serialize::from_file(common::infinit::passport_path(user_id))
            >> this->passport;

          this->hole.reset(new hole::implementations::slug::Slug(
                             storage,
                             passport,
                             Infinit::authority(),
                             reactor::network::Protocol::tcp));

          ELLE_TRACE_SCOPE("publish breached addresses to meta(%s,%s)",
                           meta_host, meta_port);
          {
            plasma::meta::Client client(meta_host, meta_port);
            try
            {
              std::vector<std::pair<std::string, uint16_t>> addresses;
              auto interfaces = elle::network::Interface::get_map(
                elle::network::Interface::Filter::only_up
                | elle::network::Interface::Filter::no_loopback
                | elle::network::Interface::Filter::no_autoip
                );
                for (auto const& pair: interfaces)
                  if (pair.second.ipv4_address.size() > 0 &&
                      pair.second.mac_address.size() > 0)
                  {
                    auto const &ipv4 = pair.second.ipv4_address;
                    addresses.emplace_back(ipv4, this->hole->port());
                  }
                ELLE_DEBUG("addresses: %s", addresses);
              std::vector<std::pair<std::string, uint16_t>> public_addresses;

              client.token(token);
              client.network_connect_device(this->descriptor.meta().id(),
                                            passport.id(),
                                            addresses,
                                            public_addresses);
            }
            catch (std::exception const& err)
            {
              ELLE_ERR("Cannot update device port: %s",
                       err.what()); // XXX[to improve]
            }
          }

          this->etoile.reset(
            new etoile::Etoile(this->identity.pair(),
                               this->hole.get(),
                               this->descriptor.meta().root()));
        });
    }

    InfinitInstanceManager::InfinitInstanceManager(std::string const& user_id,
                                                   std::string const& meta_host,
                                                   uint16_t meta_port,
                                                   std::string const& token):
      _user_id{user_id},
      _meta_host(meta_host),
      _meta_port(meta_port),
      _token{token}
    {
      ELLE_TRACE_METHOD(user_id, meta_host, meta_port, token);
    }

    InfinitInstanceManager::~InfinitInstanceManager()
    {
      ELLE_TRACE_METHOD("");

      this->clear();
    }

    void
    InfinitInstanceManager::clear()
    {
      ELLE_TRACE_METHOD("");

      auto it = begin(this->_instances);

      while(it != end(this->_instances))
      {
        this->stop(it->first);
        it = begin(this->_instances);
      }

      ELLE_ASSERT(this->_instances.empty());
    }

    void
    InfinitInstanceManager::launch(std::string const& network_id,
                                   lune::Identity const& identity,
                                   std::string const& descriptor_digest)
    {
      ELLE_TRACE_SCOPE("%s: launch network %s", *this, network_id);

      if (this->_instances.find(network_id) != this->_instances.end())
        throw elle::Exception{"Network " + network_id + " already launched"};

      std::unique_ptr<InfinitInstance> instance(
        new InfinitInstance(
          this->_user_id, this->_meta_host, this->_meta_port, this->_token,
          network_id, identity, descriptor_digest));

      this->_instances.insert(std::make_pair(network_id, std::move(instance)));
    }

    void
    InfinitInstanceManager::stop(std::string const& network_id)
    {
      ELLE_TRACE("%s: stop network %s", *this, network_id);

      ELLE_ASSERT(this->_instances.find(network_id) != this->_instances.end());

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("stop(%s)", network_id),
        [&instance]
        {
          ELLE_DEBUG("terminate all threads")
            instance.scheduler.terminate_now();
          ELLE_DEBUG("finalize etoile")
            instance.etoile.reset();
          ELLE_DEBUG("finalize hole")
            instance.hole.reset();
        });
      instance.thread.join();
      this->_instances.erase(network_id);
      ELLE_LOG("stopped");
    }

    bool
    InfinitInstanceManager::exists(std::string const& network_id) const
    {
      return this->_instances.find(network_id) != this->_instances.end();
    }

    void
    InfinitInstanceManager::add_user(
      std::string const& network_id,
      nucleus::neutron::Group::Identity const& group,
      nucleus::neutron::Subject const& subject)
    {
      ELLE_TRACE_SCOPE("%s: add user %s into network %s",
                       *this, subject, network_id);

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("add_user for %s", network_id),
        [&]
        {
          auto& etoile = *instance.etoile;

          operation_detail::user::add(etoile, group, subject);
        });
    }

    void
    InfinitInstanceManager::grant_permissions(
      std::string const& network_id,
      nucleus::neutron::Subject const& subject)
    {
      ELLE_TRACE_SCOPE("%s: grant permissions to user %s into network %s",
                       *this, subject, network_id);

      auto& instance = this->_instance(network_id);

      instance.scheduler.mt_run<void>(
        elle::sprintf("grant permissions for %s", network_id),
        [&]
        {
          auto& etoile = *instance.etoile;

          operation_detail::user::set_permissions(
            etoile, subject, nucleus::neutron::permissions::write);
        });
    }

    void
    InfinitInstanceManager::upload_files(std::string const& network_id,
                                         std::unordered_set<std::string> items,
                                         std::function<void ()> success_callback,
                                         std::function<void ()> failure_callback)

    {
      ELLE_TRACE_SCOPE("%s: uploading  %s into network %s",
                       *this, items, network_id);

      auto& instance = this->_instance(network_id);

      new reactor::Thread(
          instance.scheduler,
          elle::sprintf("upload files for %s", network_id),
          [&instance, items, success_callback, failure_callback, this]
          {
            auto& etoile = *instance.etoile;

            nucleus::neutron::Subject subject;
            subject.Create(instance.descriptor.meta().administrator_K());

            try
            {
              operation_detail::to::send(etoile, instance.descriptor, subject, items);
              ELLE_DEBUG("%s: copy succeed", *this);
              success_callback();
            }
            catch (...)
            {
              ELLE_DEBUG("%s: copy failed", *this);
              failure_callback();
              throw;
            }
          },
          true);
    }

    void
    InfinitInstanceManager::run_progress(std::string const& network_id)
    {
      ELLE_TRACE_SCOPE("%s: run progress for network %s",
                       *this, network_id);

      auto& instance = this->_instance(network_id);

      ELLE_ASSERT(instance.progress_thread == nullptr);

      auto progress_fn = [&, network_id] {

        auto& current_thread = *instance.scheduler.current();
        current_thread.wait(instance.start_progress);

        instance.progress_thread.reset(
          instance.scheduler.every(
            [&instance]
            {
              auto& etoile = *instance.etoile;
              try
              {
                float progress = operation_detail::progress::progress(etoile);
                std::lock_guard<std::mutex>(instance.progress_mutex);
                instance.progress = progress;
              }
              // XXX: catch less !
              catch (elle::Exception const&)
              {
                ELLE_WARN("couldn't retreive the progress: %s",
                          elle::exception_string());
                std::lock_guard<std::mutex>(instance.progress_mutex);
                instance.progress = 0.0f;
              }
            },
            elle::sprintf("update progress for %s", network_id),
            boost::posix_time::seconds(1)));
      };
      new reactor::Thread(instance.scheduler, __func__, progress_fn);
    }

    void
    InfinitInstanceManager::download_files(std::string const& network_id,
                                           std::vector<round> const& addresses,
                                           nucleus::neutron::Subject const& subject,
                                           std::string const& destination,
                                           std::function<void ()> success_callback,
                                           std::function<void ()> failure_callback)

    {
      ELLE_TRACE_SCOPE("%s: download files from network %s into %s",
                       *this, network_id, destination);

      auto& instance = this->_instance(network_id);

      // The progress will wait for the start_progress signal
      this->run_progress(network_id);

      new reactor::Thread(
        instance.scheduler,
        elle::sprintf("download files for %s", network_id),
        [
          &instance,
          subject,
          destination,
          success_callback,
          addresses,
          failure_callback,
          this,
          network_id
        ]
        {
          auto& etoile = *instance.etoile;
          auto& slug = dynamic_cast<hole::implementations::slug::Slug&>(*instance.hole);

          if (!this->_connect_try(slug, addresses, false))
            throw elle::Exception("Unable to connect");

          instance.start_progress.signal_one();
          try
          {
            operation_detail::from::receive(etoile, instance.descriptor, subject, destination);
            ELLE_DEBUG("%s: download succeeed", *this);
            success_callback();
          }
          catch (...)
          {
            ELLE_ERR("%s: download failed: %s", *this, elle::exception_string());
            failure_callback();
          }
        },
        true);
    }

    float
    InfinitInstanceManager::progress(std::string const& network_id)
    {
      ELLE_DEBUG_SCOPE("%s: getting progress for network %s",
                       *this, network_id);

      auto& instance = this->_instance(network_id);
      std::lock_guard<std::mutex>(instance.progress_mutex);
      return instance.progress;
    }

    bool
    InfinitInstanceManager::_connect_try(hole::implementations::slug::Slug& slug,
                                         std::vector<round> const& addresses,
                                         bool sender)
    {
      // XXX: We only use the forwarder at the moment.
      ELLE_ASSERT_EQ(sender, false);
      auto slug_connect = [&] (std::string const& endpoint)
      {
        std::vector<std::string> result;
        boost::split(result, endpoint, boost::is_any_of(":"));

        auto const &ip = result[0];
        auto const &port = result[1];
        ELLE_DEBUG("slug_connect(%s, %s)", ip, port)
        slug.portal_connect(ip, std::stoi(port), sender);
      };

      auto& sched = *reactor::Scheduler::scheduler();
      for (auto const& r: addresses)
      {
        boost::tribool succeed = boost::indeterminate;
        std::vector<std::unique_ptr<reactor::VThread<bool>>>
          connection_threads;

        for (std::string const& endpoint: r.endpoints())
        {
          auto fn = [&, endpoint] () -> bool {
            namespace slug = hole::implementations::slug;
            try
            {
              slug_connect(endpoint);
              ELLE_LOG("%s: connection to %s succeed", *this, endpoint);
              return true;
            }
            catch (slug::AlreadyConnected const& ac)
            {
              ELLE_LOG("%s: connection to %s succeed (we're already connected)",
                       *this, endpoint);
              return true;
            }
            catch (std::exception const& e)
            {
              ELLE_WARN("%s: connection to %s failed: %s", *this,
                        endpoint, elle::exception_string());
              return false;
            }
          };
          std::unique_ptr<reactor::VThread<bool>> thread_ptr{
            new reactor::VThread<bool>{
              sched,
              elle::sprintf("connect_try(%s)", endpoint),
              fn,
            },
          };
          connection_threads.push_back(std::move(thread_ptr));
        }
        for (int tries = 0; tries < 10 && indeterminate(succeed); tries++)
        {
          size_t failed = 0;
          for (auto& thread: connection_threads)
          {
            if (thread->done())
            {
              if (thread->result() == false)
                failed += 1;
              else if (thread->result() == true)
              {
                succeed = true;
                break;
              }
            }
            if (failed == connection_threads.size())
            {
              succeed = false;
              break;
            }
          }
          reactor::Sleep pause{sched, 1_sec};
          pause.run();
        }
        for (auto& thread: connection_threads)
          thread->terminate_now();
        if (succeed)
        {
          // Connection successful
          ELLE_TRACE("connection round(%s) successful", r.endpoints());
          return true;
        }
        else if (not succeed)
        {
          // Connection failed
          ELLE_TRACE("connection round(%s) failed", r.endpoints());
          continue;
        }
        else
        {
          // Connection status failed to be determined this is a timeout
          ELLE_TRACE("connection round(%s) timeout", r.endpoints());
          continue;
        }
      }
      return false;
    }

    void
    InfinitInstanceManager::connect_try(std::string const& network_id,
                                        std::vector<round> const& addresses,
                                        bool sender)
    {
      ELLE_TRACE_SCOPE("%s: connecting infinit of network %s to %s ",
                       *this, network_id, addresses);

      auto& instance = this->_instance(network_id);
      auto& slug = dynamic_cast<hole::implementations::slug::Slug&>(*instance.hole);

      // return instance.scheduler.mt_run<bool>(
      //   elle::sprintf("connecting nodes for %s", network_id),
      new reactor::Thread(
        instance.scheduler,
        elle::sprintf("connecting nodes for %s", network_id),
        [&, addresses, sender]
        {
          if (!this->_connect_try(slug, addresses, sender))
            throw elle::Exception("Unable to connect");
          // The progress will wait for the start_progress signal
          instance.start_progress.signal_one();
        }, true);
    }

    InfinitInstance&
    InfinitInstanceManager::_instance(std::string const& network_id)
    {
      auto it = this->_instances.find(network_id);
      if (it == this->_instances.end())
        throw elle::Exception{"Cannot find any network " + network_id};
      ELLE_ASSERT_NEQ(it->second, nullptr);
      return *(it->second);
    }

    InfinitInstance const*
    InfinitInstanceManager::_instance_for_file(std::string const& path)
    {
      ELLE_TRACE_METHOD(path);

      for (auto const& pair : this->_instances)
      {
        std::string mount_point = pair.second->mount_point;

        if (path.compare(0, mount_point.length(), path) != 0)
          return pair.second.get();
      }
      return nullptr;
    }

    /*----------.
    | Printable |
    `----------*/
    void
    InfinitInstanceManager::print(std::ostream& stream) const
    {
      stream << "InstanceManager(" << this->_user_id << ")";
    }
  }
}
