#include <elle/Lazy.hh>
#include <elle/finally.hh>
#include <elle/log.hh>

#include <reactor/scheduler.hh>
#include <reactor/network/exception.hh>

#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.Client")

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        Client::Client(Trophonius& trophonius,
                       std::unique_ptr<reactor::network::TCPSocket>&& socket):
          _trophonius(trophonius),
          _socket(std::move(socket)),
          _handle_thread(*reactor::Scheduler::scheduler(),
                         elle::sprintf("%s handler", *this),
                         std::bind(&Client::handle, std::ref(*this)))
        {}

        Client::~Client()
        {
          ELLE_LOG("%s: remove", *this);
          this->_handle_thread.terminate_now();
        }

        void
        Client::handle()
        {
          RemoveWard ward(*this);
          this->_handle();
        }

        /*-----.
        | Ward |
        `-----*/

        Client::RemoveWard::RemoveWard(Client& c):
          elle::SafeFinally(
            [&c]
            {
              c.trophonius().client_remove(c);
            })
        {}

        /*----------.
        | Printable |
        `----------*/

        void
        Client::print(std::ostream& stream) const
        {
          stream << "Client(" << this->_socket->peer() << ")";
        }
      }
    }
  }
}
