#include <elle/Lazy.hh>
#include <elle/finally.hh>
#include <elle/log.hh>

#include <reactor/scheduler.hh>
#include <reactor/network/exception.hh>

#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        Client::Client(Trophonius& trophonius,
                       std::unique_ptr<reactor::network::Socket>&& socket):
          _trophonius(trophonius),
          _socket(std::move(socket)),
          _handle_thread(*reactor::Scheduler::scheduler(),
                         elle::sprintf("%s handler", *this),
                         std::bind(&Client::handle, std::ref(*this)))
        {}

        Client::~Client()
        {
          this->_terminate();
        }

        void
        Client::handle()
        {
          RemoveWard ward(*this);
          this->_handle();
        }

        void
        Client::terminate()
        {
          this->_terminate();
        }

        void
        Client::unregister()
        {
          this->_unregister();
        }

        void
        Client::_terminate()
        {
          this->_handle_thread.terminate_now(false);
        }

        void
        Client::_unregister()
        {}

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
