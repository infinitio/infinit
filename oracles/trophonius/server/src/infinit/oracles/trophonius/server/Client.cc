#include <jsoncpp/json/reader.h>

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
        class ProtocolError:
            public elle::Exception
        {
        public:
          ProtocolError(std::string const& message):
            elle::Exception(message)
          {}
        };

        class RemoveWard:
          public elle::SafeFinally
        {
        public:
          RemoveWard(Client& c):
            elle::SafeFinally(
              [&c]
              {
                c.trophonius().client_remove(c);
              })
          {}
        };

        Client::Client(Trophonius& trophonius,
                       std::unique_ptr<reactor::network::TCPSocket>&& socket):
          _trophonius(trophonius),
          _socket(std::move(socket)),
          _handle_thread(*reactor::Scheduler::scheduler(),
                         elle::sprintf("%s handler", *this),
                         std::bind(&Client::_handle, std::ref(*this))),
          _authentified(false),
          _ping_thread(*reactor::Scheduler::scheduler(),
                       elle::sprintf("%s ping", *this),
                       std::bind(&Client::_ping, std::ref(*this))),
          _pong_thread(*reactor::Scheduler::scheduler(),
                       elle::sprintf("%s pong", *this),
                       std::bind(&Client::_pong, std::ref(*this))),
          _pinged(false)
        {
          ELLE_LOG("%s: accept connection", *this);
        }

        Client::~Client()
        {
          ELLE_LOG("%s: remove", *this);
          this->_handle_thread.terminate_now();
          this->_ping_thread.terminate_now();
          this->_pong_thread.terminate_now();
        }

        static
        Json::Value
        read_json(reactor::network::TCPSocket& socket)
        {
          auto buffer = socket.read_until("\n");
          Json::Value root;
          Json::Reader reader;
          bool res = reader.parse(
            reinterpret_cast<char*>(buffer.contents()),
            reinterpret_cast<char*>(buffer.contents()) + buffer.size(),
            root, false);
          if (!res)
          {
            auto error = reader.getFormatedErrorMessages();
            throw ProtocolError(elle::sprintf("JSON error: %s", error));
          }
          return root;
        }

        void
        Client::_handle()
        {
          RemoveWard ward(*this);
          try
          {
            while (true)
            {
              auto json = read_json(*this->_socket);
              if (json.isMember("notification_type"))
              {
                this->_pinged = true;
              }
              else
              {
                if (!json.isMember("user_id") ||
                    !json.isMember("token") ||
                    !json.isMember("device_id"))
                {
                  throw ProtocolError(
                    elle::sprintf("unrecognized json message: %s",
                                  json.toStyledString()));
                }
                this->_authentified = true;
              }
            }
          }
          catch (ProtocolError const& e)
          {
            ELLE_WARN("%s: protocol error: %s", *this, e.what());
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: network error: %s", *this, e.what());
          }
        }

        void
        Client::_ping()
        {
          RemoveWard ward(*this);
          static std::string const ping_msg("{\"notification_type\": 208}\n");
          try
          {
            while (true)
            {
              ELLE_TRACE("%s: send ping", *this);
              this->_socket->write(ping_msg);
              reactor::sleep(this->_trophonius.ping_period());
            }
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: network error: %s", *this, e.what());
          }
        }

        void
        Client::_pong()
        {
          RemoveWard ward(*this);
          auto period = this->_trophonius.ping_period() * 2;
          while (true)
          {
            reactor::sleep(period);
            if (!this->_pinged)
            {
              ELLE_WARN("%s: didn't receive ping after %s", *this, period);
              break;
            }
            this->_pinged = false;
            ELLE_TRACE("%s: send ping", *this);
          }
        }

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
