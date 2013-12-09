#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <elle/Lazy.hh>
#include <elle/log.hh>

#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>

#include <infinit/oracles/trophonius/server/Trophonius.hh>
#include <infinit/oracles/trophonius/server/User.hh>
#include <infinit/oracles/trophonius/server/exceptions.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.User")

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
          // Session
          _device_id(boost::uuids::nil_uuid()),
          _user_id(),
          _session_id(),
          // Meta
          _meta(this->trophonius().meta().host(),
                this->trophonius().meta().port()),
          _authentified(),
          _registered(false),
          // Notifications
          _notifications(),
          _notification_available(),
          _notifications_thread(*reactor::Scheduler::scheduler(),
                                elle::sprintf("%s notifications", *this),
                                std::bind(&User::_handle_notifications,
                                          std::ref(*this))),
          // Ping pong
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

          this->_terminate();
        }

        void
        User::_unregister()
        {
          if (!this->_registered)
            return;
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
        User::_terminate()
        {
          this->_notifications_thread.terminate_now(false);
          this->_ping_thread.terminate_now(false);
          this->_pong_thread.terminate_now(false);
          Super::_terminate();
        }

        /*--------------.
        | Notifications |
        `--------------*/

        class LazyJson:
          public elle::Lazy<std::string>
        {
        public:
          LazyJson(elle::json::Object const& object):
            Lazy(std::bind(&elle::json::pretty_print_json, std::ref(object)))
          {}
        };

        void
        User::notify(elle::json::Object const& notification)
        {
          LazyJson json(notification);
          ELLE_DEBUG("%s: push notification: %s", *this, json);
          this->_notifications.push(std::move(notification));
          this->_notification_available.signal();
        }

        void
        User::_handle_notifications()
        {
          this->_authentified.wait();
          RemoveWard ward(*this);
          try
          {
            ELLE_DEBUG_SCOPE("%s: start handling notifications", *this);
            while (true)
            {
              while (!this->_notifications.empty())
              {
                auto notification = std::move(this->_notifications.front());
                this->_notifications.pop();
                LazyJson json(notification);
                ELLE_TRACE("%s: send notification: %s", *this, json)
                  elle::json::write_json(*this->_socket, notification);
              }
              this->_notification_available.wait();
            }
          }
          catch (reactor::network::Exception const& e)
          {
            ELLE_WARN("%s: network error: %s", *this, e.what());
          }
        }

        /*-------.
        | Server |
        `-------*/

        void
        User::_handle()
        {
          try
          {
            while (true)
            {
              auto json = elle::json::read_json(*this->_socket);
              ELLE_DUMP("%s: receive packet: %s", *this, LazyJson(json));
              if (json.find("notification_type") != json.end())
              {
                ELLE_DEBUG("%s: receive ping", *this);
                this->_pinged = true;
                continue;
              }
              else if (json.find("user_id") != json.end() &&
                       json.find("session_id") != json.end() &&
                       json.find("device_id") != json.end())
              {
                try
                {
                  this->_user_id = boost::any_cast<std::string>(json["user_id"]);
                  this->_device_id =
                    boost::uuids::string_generator()(
                      boost::any_cast<std::string>(json["device_id"]));
                  this->_meta.session_id(
                    boost::any_cast<std::string>(json["session_id"]));
                }
                catch (std::runtime_error const& e)
                {
                  throw ProtocolError(
                    elle::sprintf("ids must be strings: %s",
                                  elle::json::pretty_print_json(json)));
                }
                ELLE_TRACE_SCOPE("%s: connect with user %s and device %s",
                                 *this, this->_user_id, this->_device_id);
                // Remove potential stray previous client.
                // XXX: this removes the previous client even if the fails at
                // login.
                {
                  auto& users = this->trophonius().users().get<1>();
                  auto it = users.find(
                    boost::make_tuple(this->_user_id, this->_device_id));
                  if (it != users.end())
                  {
                    auto previous = *it;
                    ELLE_LOG("%s: remove previous instance %s",
                             *this, *previous);
                    users.erase(it);
                    previous->terminate();
                    delete previous;
                  }
                }
                this->trophonius().users().insert(this);
                this->trophonius().users_pending().erase(this);
                meta::Response res;
                try
                {
                  res = this->_meta.connect(this->trophonius().uuid(),
                                            this->_user_id,
                                            this->_device_id);
                }
                catch (infinit::oracles::meta::Exception const& e)
                {
                  // FIXME: the meta client exception is bullshit.
                  if (e.err == infinit::oracles::meta::Error::unknown)
                    throw;
                  else
                    throw AuthenticationError(e.what());
                }
                ELLE_TRACE("%s: authentified", *this);
                this->_registered = true;
                elle::json::Object response;
                response["notification_type"] = int(-666);
                response["response_code"] = int(200);
                response["response_details"] = std::string(res.error_details);
                elle::json::write_json(*this->_socket, response);
                this->_authentified.open();
                continue;
              }
              else if (json.find("poke") != json.end())
              {
                ELLE_DEBUG_SCOPE("%s: handle poke", *this);
                auto poke = json["poke"];
                ELLE_DUMP("%s: poke: %s",
                          *this,
                          boost::any_cast<std::string>(poke));
                elle::json::write_json(*this->_socket, json);
              }
              else
                throw ProtocolError(
                  elle::sprintf("unrecognized json message: %s",
                                elle::json::pretty_print_json(json)));
            }
          }
          catch (ProtocolError const& e)
          {
            ELLE_WARN("%s: protocol error: %s", *this, e.what());
          }
          catch (AuthenticationError const& e)
          {
            ELLE_WARN("%s: authentication error: %s", *this, e.what());
            elle::json::Object response;
            response["notification_type"] = int(-666);
            response["response_code"] = int(403);
            response["response_details"] = std::string(e.what());
            elle::json::write_json(*this->_socket, response);
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
        User::_ping()
        {
          RemoveWard ward(*this);
          elle::SafeFinally desauthenticate(
            [&]
            {
              this->_authentified.close();
            });
          static elle::json::Object const ping_msg(
            {{std::string("notification_type"), int(208)}}
          );
          try
          {
            while (true)
            {
              this->_authentified.wait();
              reactor::sleep(this->trophonius().ping_period());
              ELLE_DEBUG("%s: send ping", *this);
              elle::json::write_json(*this->_socket, ping_msg);
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
            stream << " " << this->user_id()
                   << " on device " << this->device_id();
          stream << ")";
        }
      }
    }
  }
}
