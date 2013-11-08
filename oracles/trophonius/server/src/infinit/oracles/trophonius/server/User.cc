#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <jsoncpp/json/writer.h>

#include <elle/log.hh>

#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>

#include <infinit/oracles/trophonius/server/Trophonius.hh>
#include <infinit/oracles/trophonius/server/User.hh>
#include <infinit/oracles/trophonius/server/exceptions.hh>
#include <infinit/oracles/trophonius/server/utils.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.Meta")

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        User::User(Trophonius& trophonius,
                   std::unique_ptr<reactor::network::TCPSocket>&& socket):
          Client(trophonius, std::move(socket)),
          _authentified(),
          _meta(this->trophonius().meta().host(),
                this->trophonius().meta().port()),
          _device_id(boost::uuids::nil_uuid()),
          _user_id(),
          _session_id(),
          _ping_thread(*reactor::Scheduler::scheduler(),
                       elle::sprintf("%s ping", *this),
                       std::bind(&User::_ping, std::ref(*this))),
          _pong_thread(*reactor::Scheduler::scheduler(),
                       elle::sprintf("%s pong", *this),
                       std::bind(&User::_pong, std::ref(*this))),
          _pinged(false)
        {
          ELLE_TRACE("%s: connected", *this);
        }

        User::~User()
        {
          ELLE_TRACE("%s: disconnected", *this);
          this->_ping_thread.terminate_now();
          this->_pong_thread.terminate_now();
          try
          {
            auto res = this->_meta.disconnect(
              this->trophonius().uuid(), this->_user_id, this->_device_id);
            if (!res.success())
            {
              // XXX.
              ELLE_WARN("%s: unable to disconnect user: %s",
                        *this, res.error_details);
            }
          }
          catch (reactor::network::Exception const& e) // XXX.
          {
            ELLE_WARN("%s: unable to disconnect user: network error: %s",
                      *this, e.what());
          }
          catch (std::exception const& e)
          {
            ELLE_WARN("%s: unable to disconnect user: %s", *this, e.what());
          }
        }

        void
        User::notify(Json::Value const& notification)
        {
          Json::FastWriter writer;
          auto notif = writer.write(notification);
          ELLE_TRACE("%s: send notification: %s", *this, notif);
          this->_socket->write(notif);
        }

        void
        User::_handle()
        {
          try
          {
            while (true)
            {
              auto json = read_json(*this->_socket);
              if (json.isMember("notification_type") )
              {
                ELLE_DEBUG("%s: receive ping", *this);
                this->_pinged = true;
              }
              else
              {
                if (!json.isMember("user_id") ||
                    !json.isMember("session_id") ||
                    !json.isMember("device_id"))
                {
                  throw ProtocolError(
                    elle::sprintf("unrecognized json message: %s",
                                  json.toStyledString()));
                }
                this->_user_id = json["user_id"].asString();
                this->_device_id =
                  boost::uuids::string_generator()(
                    json["device_id"].asString());
                this->_meta.session_id(json["session_id"].asString());
                this->_connect();
              }
            }
          }
          catch (ProtocolError const& e)
          {
            ELLE_WARN("%s: protocol error: %s", *this, e.what());
          }
          catch (AuthenticationError const& e)
          {
            ELLE_WARN("%s: authentication error: %s", *this, e.what());
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: network error: %s", *this, e.what());
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          catch (...)
          {
            ELLE_WARN("%s: unknown error: %s", *this, elle::exception_string());
          }
        }

        void
        User::_connect()
        {
          auto res = this->_meta.connect(this->trophonius().uuid(),
                                         this->_user_id,
                                         this->_device_id);

          if (!res.success())
            throw AuthenticationError(elle::sprintf("%s", res.error_details));

          Json::Value response;
          response["notification_type"] = -666;
          response["response_code"] = 200;
          response["response_details"] = res.error_details;

          Json::FastWriter writer;
          this->_socket->write(writer.write(response));
          ELLE_TRACE("%s: authentified", *this);

          this->_authentified.open();
        }

        void
        User::_ping()
        {
          RemoveWard ward(*this);
          elle::SafeFinally desauthenticate(
            [&]
            {
              this->_authentified.close();
            });
          static std::string const ping_msg("{\"notification_type\": 208}\n");
          try
          {
            while (true)
            {
              this->_authentified.wait();
              ELLE_DEBUG("%s: send ping", *this);
              this->_socket->write(ping_msg);
              reactor::sleep(this->trophonius().ping_period());
            }
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: network error: %s", *this, e.what());
          }
        }

        void
        User::_pong()
        {
          RemoveWard ward(*this);
          elle::SafeFinally desauthenticate{[&] { this->_authentified.close(); }};
          auto period = this->trophonius().ping_period() * 2;
          while (true)
          {
            this->_authentified.wait();
            reactor::sleep(period);
            if (!this->_pinged)
            {
              ELLE_WARN("%s: didn't receive ping after %s", *this, period);
              break;
            }
            this->_pinged = false;
          }
        }

        /*----------.
        | Printable |
        `----------*/
        void
        User::print(std::ostream& stream) const
        {
          stream << "User(" << this->_socket->peer();
          if (this->device_id() != boost::uuids::nil_uuid())
            stream << " on device " << this->device_id() << ")";
        }
      }
    }
  }
}
