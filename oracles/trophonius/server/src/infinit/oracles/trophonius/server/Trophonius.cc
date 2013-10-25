#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

#include <boost/uuid/random_generator.hpp>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.Trophonius")

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        Trophonius::Trophonius(
          int port,
          std::string const& meta_host,
          int meta_port,
          boost::posix_time::time_duration const& ping_period):
          _server(),
          _notifications(),
          _accepter(*reactor::Scheduler::scheduler(),
                    elle::sprintf("%s accepter", *this),
                    std::bind(&Trophonius::_serve, std::ref(*this))),
          _uuid(boost::uuids::random_generator()()),
          _meta(meta_host, meta_port),
          _ping_period(ping_period)
        {
          elle::SafeFinally kill_accepter{
            [&] { this->_accepter.terminate_now(); }
          };

          try
          {
            this->_server.listen(port);
            ELLE_LOG("%s: listen on port %s (users)", *this, this->port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to listen on port %s (users): %s",
                     *this, port, elle::exception_string());
            throw;
          }

          try
          {
            this->_notifications.listen();
            ELLE_LOG("%s: listen notification: %s", *this, this->notification_port());
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
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to register to meta: %s",
                     *this, elle::exception_string());
            throw;
          }

          kill_accepter.abort();
        }

        Trophonius::~Trophonius()
        {
          ELLE_LOG("%s: terminate", *this);
          this->_accepter.terminate_now();

          this->_meta.unregister_trophonius(this->_uuid);
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
            this->_clients.emplace(new Client(*this, std::move(socket)));
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
              elle::sprintf("remove %s", c),
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
