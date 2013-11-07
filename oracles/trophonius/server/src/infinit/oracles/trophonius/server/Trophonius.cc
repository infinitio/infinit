#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/trophonius/server/Meta.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>
#include <infinit/oracles/trophonius/server/User.hh>

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
        UnknownClient::UnknownClient(boost::uuids::uuid const& device_id):
          elle::Exception(elle::sprintf("Unknown client: %s", device_id))
        {}

        Trophonius::Trophonius(
          int port,
          std::string const& meta_host,
          int meta_port,
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
          _ping_period(ping_period)
        {
          elle::SafeFinally kill_accepters{
            [&]
            {
              this->_accepter.terminate_now();
              this->_meta_accepter.terminate_now();
              this->_signal();
            }
          };

          try
          {
            this->_server.listen(this->_port);
            ELLE_LOG("%s: listen on port %s (users)", *this, this->port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to listen on port %s (users): %s",
                     *this, this->_port, elle::exception_string());
            throw;
          }

          try
          {
            this->_notifications.listen();
            ELLE_LOG("%s: listen notification: %s",
                     *this, this->notification_port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to listen notifications: %s",
                     *this, elle::exception_string());
            throw;
          }

          try
          {
            this->_meta.register_trophonius(
              this->_uuid, this->notification_port());
            ELLE_LOG("%s: registered to meta as %s on port %s",
                     *this, this->_uuid, this->notification_port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to register to meta: %s",
                     *this, elle::exception_string());
            throw;
          }

          kill_accepters.abort();
        }

        void
        Trophonius::stop()
        {
          this->_signal();
        }

        Trophonius::~Trophonius()
        {
          ELLE_LOG("%s: terminate", *this);
          this->_accepter.terminate_now();
          this->_meta_accepter.terminate_now();

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
            this->_clients.emplace(new User(*this, std::move(socket)));
          }
        }

        User&
        Trophonius::user(boost::uuids::uuid const& device)
        {
          auto client = std::find_if(
            this->_clients.begin(),
            this->_clients.end(),
            [&device] (Client* client)
            {
              if (dynamic_cast<User*>(client) == nullptr)
                return false;

              User const* user = static_cast<User const*>(client);
              return user->device_id() == device;
            });

          if (client == this->_clients.end())
            throw UnknownClient(device);

          ELLE_ASSERT(dynamic_cast<User*>(*client) != nullptr);
          return *static_cast<User*>(*client);
        }

        void
        Trophonius::_serve_notifier()
        {
          while (true)
          {
            std::unique_ptr<reactor::network::TCPSocket> socket(
              this->_notifications.accept());
            this->_clients.emplace(new Meta(*this, std::move(socket)));
          }
        }

        /*--------.
        | Clients |
        `--------*/
        void
        Trophonius::client_remove(Client& c)
        {
          if (this->_clients.erase(&c))
            reactor::run_later(
              elle::sprintf("remove client %s", c),
              [&c]
              {
                delete &c;
              });
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
