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
                   std::unique_ptr<reactor::network::Socket>&& socket):
          Client(trophonius, std::move(socket)),
          // Session
          _device_id(boost::uuids::nil_uuid()),
          _user_id(),
          _session_id(),
          _version(),
          _os(),
          // Meta
          _meta(this->trophonius().meta().protocol(),
                this->trophonius().meta().host(),
                this->trophonius().meta().port()),
          _authentified(),
          _registered(false),
          // Notifications
          _notifications(),
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
          // disconnect if the user takes too long to register
          _set_timeout(this->trophonius().user_auth_max_time());
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
          LazyJson(boost::any const& object):
            Lazy(std::bind(&elle::json::pretty_print, std::ref(object)))
          {}
        };

        void
        User::notify(elle::json::Object const& notification)
        {
          LazyJson json(notification);
          ELLE_DEBUG("%s: push notification: %s", *this,
                     elle::json::pretty_print(notification));
          this->_notifications.put(std::move(notification));
        }

        void
        User::_handle_notifications()
        {
          this->_authentified.wait();
          elle::With<RemoveWard>(*this) << [&](RemoveWard&)
          {
            try
            {
              ELLE_DEBUG_SCOPE("%s: start handling notifications", *this);
              while (true)
              {
                auto notification = this->_notifications.get();
                // Construct any object so that json object isn't destroyed
                // before being printed.
                boost::any any(notification);
                LazyJson json(any);
                ELLE_TRACE("%s: send notification: %s", *this, json)
                elle::json::write(*this->_socket, notification);
              }
            }
            catch (reactor::network::Exception const& e)
            {
              ELLE_WARN("%s: network error: %s", *this, e.what());
            }
          };
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
              auto json_read = elle::json::read(*this->_socket);
              auto json = boost::any_cast<elle::json::Object>(json_read);
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
                  if (json.find("version") != json.end())
                  {
                    auto const& version =
                      boost::any_cast<elle::json::Object>(json.at("version"));
                    try
                    {
                      auto major = boost::any_cast<int64_t>(version.at("major"));
                      auto minor = boost::any_cast<int64_t>(version.at("minor"));
                      auto subminor = boost::any_cast<int64_t>(version.at("subminor"));
                      this->_version = elle::Version(major, minor, subminor);
                      ELLE_LOG("%s: client version: %s", *this, this->_version);
                    }
                    catch (...)
                    {
                      ELLE_WARN("%s: unable to get client version: %s",
                                *this, elle::exception_string());
                    }
                  }
                  else
                    ELLE_WARN("%s: client didn't send his version", *this);
                  if (json.find("os") != json.end())
                  {
                    this->_os =
                      boost::any_cast<std::string>(json.at("os"));
                  }
                  else
                    ELLE_WARN("%s: client didn't send his os", *this);
                }
                catch (std::runtime_error const& e)
                {
                  throw ProtocolError(
                    elle::sprintf("ids must be strings: %s",
                                  elle::json::pretty_print(json)));
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
                this->_cancel_timeout();
                meta::Response res;
                try
                {
                  this->_meta.connect(this->trophonius().uuid(),
                                      this->_user_id,
                                      this->_device_id,
                                      this->_version,
                                      this->_os);
                }
                // FIXME: the meta client exception is bullshit.
                catch (elle::http::Exception const& e)
                {
                  if (e.code == elle::http::ResponseCode::forbidden ||
                      e.code == elle::http::ResponseCode::internal_server_error ||
                      e.code == elle::http::ResponseCode::unknown_error)
                  {
                    throw AuthenticationError(e.what());
                  }
                  else
                  {
                    throw;
                  }
                }
                ELLE_TRACE("%s: authentified", *this);
                this->_registered = true;
                elle::json::Object response;
                response["notification_type"] = int(-666);
                response["response_code"] = int(200);
                response["response_details"] = std::string(res.error_details);
                elle::json::write(*this->_socket, response);
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
                elle::json::write(*this->_socket, json);
              }
              else
                throw ProtocolError(
                  elle::sprintf("unrecognized json message: %s",
                                elle::json::pretty_print(json)));
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
            response["notification_type"] = int(12);
            response["response_code"] = int(403);
            response["response_details"] = std::string(e.what());
            elle::json::write(*this->_socket, response);
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
          elle::With<RemoveWard>(*this) << [&](RemoveWard&)
          {
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
                elle::json::write(*this->_socket, ping_msg);
              }
            }
            catch (reactor::network::Exception const& e)
            {
              ELLE_WARN("%s: network error: %s", *this, e.what());
            }
          };
        }

        void
        User::_pong()
        {
          elle::With<RemoveWard>(*this) << [&](RemoveWard&)
          {
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
          };
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
