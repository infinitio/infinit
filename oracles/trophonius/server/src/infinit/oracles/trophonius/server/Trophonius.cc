#include <boost/functional/hash.hpp>

#include <elle/log.hh>

#include <reactor/scheduler.hh>
#include <reactor/exception.hh>

#include <infinit/oracles/trophonius/server/Meta.hh>
#include <infinit/oracles/trophonius/server/User.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.Trophonius")

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        UnknownClient::UnknownClient(std::string const& user_id,
                                     boost::uuids::uuid const& device_id):
          elle::Exception(elle::sprintf("Unknown client: %s:%s",
                                        user_id, device_id))
        {}

        Trophonius::Trophonius(
          int port,
          std::string const& meta_host,
          int meta_port,
          int notifications_port,
          boost::posix_time::time_duration const& user_ping_period,
          boost::posix_time::time_duration const& ping_period):
          Waitable("trophonius"),
          _server(),
          _port(port),
          _notifications(),
          _accepter(
            *reactor::Scheduler::scheduler(),
            elle::sprintf("%s accepter", *this),
            std::bind(&Trophonius::_serve, std::ref(*this))),
          _meta_accepter(
            *reactor::Scheduler::scheduler(),
            elle::sprintf("%s meta accepter", *this),
            std::bind(&Trophonius::_serve_notifier, std::ref(*this))),
          _uuid(boost::uuids::random_generator()()),
          _meta(meta_host, meta_port),
          _meta_pinger(
            reactor::Scheduler::scheduler()->every(
              [&]
              {
                this->_ready.wait();
                try
                {
                  this->_meta.register_trophonius(
                    this->_uuid, this->notification_port());
                }
                catch (elle::Exception const& e)
                {
                  ELLE_ERR("%s: unable to ping: %s", *this, e.what());
                }
              },
              "pinger",
              ping_period
              )
            ),
          _ping_period(user_ping_period)
        {
          elle::SafeFinally kill_accepters{
            [&]
            {
              this->_accepter.terminate_now();
              this->_meta_accepter.terminate_now();
              this->_meta_pinger->terminate_now();
              this->_signal();
            }
          };

          try
          {
            this->_server.listen(this->_port);
            this->_port = this->_server.port();
            ELLE_LOG("%s: listen for users on port %s", *this, this->port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to listen on port %s (users): %s",
                     *this, this->_port, elle::exception_string());
            throw;
          }

          try
          {
            this->_notifications.listen(notifications_port);
            ELLE_LOG("%s: listen for meta on port %s",
                     *this, this->notification_port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to listen notifications: %s",
                     *this, elle::exception_string());
            throw;
          }

          ELLE_LOG("%s: register to meta", *this)
            this->_meta.register_trophonius(
              this->_uuid, this->notification_port());
          this->_ready.open();

          kill_accepters.abort();
        }

        void
        Trophonius::stop()
        {
          this->_signal();
        }

        Trophonius::~Trophonius()
        {
          this->_accepter.terminate_now();
          this->_meta_accepter.terminate_now();
          this->_meta_pinger->terminate_now();

          while (!this->_users.empty())
          {
            // Remove the client from the set first or it will try to clean
            // itself up.
            auto it = this->_users.begin();
            auto client = *it;
            this->_users.erase(it);
            client->terminate();
            delete client;
          }
          while (!this->_users_pending.empty())
          {
            // Remove the client from the set first or it will try to clean
            // itself up.
            auto it = this->_users_pending.begin();
            auto client = *it;
            this->_users_pending.erase(it);
            client->terminate();
            delete client;
          }
          while (!this->_metas.empty())
          {
            // Remove the client from the set first or it will try to clean
            // itself up.
            auto it = this->_metas.begin();
            auto client = *it;
            this->_metas.erase(it);
            client->terminate();
            delete client;
          }

          try
          {
            this->_meta.unregister_trophonius(this->_uuid);
            ELLE_LOG("%s: unregistered from meta", *this);
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to unregister from meta: %s",
                     *this, elle::exception_string());
            throw;
          }
        }

        /*-------.
        | Server |
        `-------*/

        int
        Trophonius::port() const
        {
          return this->_server.port();
        }

        int
        Trophonius::notification_port() const
        {
          return this->_notifications.port();
        }

        void
        Trophonius::_serve()
        {
          while (true)
          {
            std::unique_ptr<reactor::network::TCPSocket> socket(
              this->_server.accept());
            auto user = new User(*this, std::move(socket));
            this->_users_pending.insert(user);
          }
        }

        User&
        Trophonius::user(std::string const& user_id,
                         boost::uuids::uuid const& device)
        {
          auto& index = this->_users.get<1>();
          boost::tuple<std::string, boost::uuids::uuid> key(user_id, device);
          auto it = index.find(key);
          if (it == index.end())
            throw UnknownClient(user_id, device);
          return **it;
        }

        void
        Trophonius::_serve_notifier()
        {
          while (true)
          {
            std::unique_ptr<reactor::network::TCPSocket> socket(
              this->_notifications.accept());
            this->_metas.emplace(new Meta(*this, std::move(socket)));
          }
        }

        /*--------.
        | Clients |
        `--------*/

        // std::size_t
        // Trophonius::hash_user_device::operator()(User* user) const
        // {
        //   auto key = std::make_pair(user->user_id(), user->device_id());
        //   boost::hash<decltype(key)> hasher;
        //   return hasher(key);
        // }

        void
        Trophonius::client_remove(Client& c)
        {
          // Remove the client from the set first to ensure other cleanup do
          // not duplicate this.
          if (this->_users.erase(static_cast<User*>(&c)) ||
              this->_users_pending.erase(static_cast<User*>(&c)))
          {
            // Terminate all handlers for the clients. We are most likely
            // invoked by one of those handler, so they must take care of not
            // commiting suicide.
            c.terminate();
            // Unregister the client from meta.
            static_cast<User&>(c).unregister();
            // Delete the client later since we are probably since inside one of
            // its handlers and it would cause premature destruction of this
            // thread.
            reactor::run_later(
              elle::sprintf("remove client %s", c),
              [&c] {delete &c;});
          }
          else if (this->_metas.erase(static_cast<Meta*>(&c)))
          {
            c.terminate();
            reactor::run_later(
              elle::sprintf("remove client %s", c),
              [&c] {delete &c;});
          }
        }

        /*----------.
        | Printable |
        `----------*/

        void
        Trophonius::print(std::ostream& stream) const
        {
          stream << "Trophonius";
        }
      }
    }
  }
}
