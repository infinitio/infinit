#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

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
          boost::posix_time::time_duration const& ping_period):
          _server(),
          _accepter(*reactor::Scheduler::scheduler(),
                    elle::sprintf("%s accepter", *this),
                    std::bind(&Trophonius::_serve, std::ref(*this))),
          _ping_period(ping_period)
        {
          try
          {
            this->_server.listen(port);
            ELLE_LOG("%s: listen on port %s", *this, this->port());
          }
          catch (...)
          {
            ELLE_ERR("%s: unable to listen on port %s", *this, port);
            this->_accepter.terminate_now();
            throw;
          }
        }

        Trophonius::~Trophonius()
        {
          ELLE_LOG("%s: terminate", *this);
          this->_accepter.terminate_now();
        }

        /*-------.
        | Server |
        `-------*/

        int
        Trophonius::port() const
        {
          return this->_server.port();
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
