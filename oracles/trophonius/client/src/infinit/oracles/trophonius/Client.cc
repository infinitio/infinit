#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <iostream>

#include <boost/random.hpp>

#include <elle/Buffer.hh>
#include <elle/assert.hh>
#include <elle/cast.hh>
#include <elle/finally.hh>
#include <elle/format/json/Array.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/format/json/Object.hh>
#include <elle/format/json/Parser.hh>
#include <elle/json/exceptions.hh>
#include <elle/json/json.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/print.hh>
#include <elle/serialization/json.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/NamedValue.hh>
#include <elle/serialize/Serializer.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/system/platform.hh>

#include <reactor/Channel.hh>
#include <reactor/Scope.hh>
#include <reactor/TimeoutGuard.hh>
#include <reactor/exception.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/fingerprinted-socket.hh>
#include <reactor/network/resolve.hh>
#include <reactor/scheduler.hh>
#include <reactor/signal.hh>
#include <reactor/thread.hh>

#include <infinit/oracles/trophonius/Client.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.Client");

#ifdef INFINIT_WINDOWS
# define SUICIDE() ::exit(0);
#else
# define SUICIDE() ::kill(::getpid(), SIGKILL)
#endif

/*-------------------------.
| Notification serializers |
`-------------------------*/
ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::Notification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::Notification, ar, value, version)
{
  (void)version;
  ar & named("notification_type", value.notification_type);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::ConnectionEnabledNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::ConnectionEnabledNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & named("response_code", value.response_code);
  ar & named("response_details", value.response_details);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::NewSwaggerNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::NewSwaggerNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & named("user_id", value.user_id);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::DeletedSwaggerNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::DeletedSwaggerNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & named("user_id", value.user_id);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::DeletedFavoriteNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::DeletedFavoriteNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & named("user_id", value.user_id);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::UserStatusNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::UserStatusNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & named("user_id", value.user_id);
  ar & named("status", value.user_status);
  ar & named("device_id", value.device_id);
  ar & named("device_status", value.device_status);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::LinkTransactionNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::LinkTransactionNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & base_class<infinit::oracles::LinkTransaction>(value);
}

ELLE_SERIALIZE_NO_FORMAT(infinit::oracles::trophonius::MessageNotification);
ELLE_SERIALIZE_SIMPLE(infinit::oracles::trophonius::MessageNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<infinit::oracles::trophonius::Notification>(value);
  ar & named("sender_id", value.sender_id);
  ar & named("message", value.message);
}

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      boost::posix_time::time_duration const default_ping_period(
        boost::posix_time::seconds(30));

      /*--------------.
      | Notifications |
      `--------------*/

      PeerTransactionNotification::PeerTransactionNotification(
        elle::serialization::SerializerIn& input)
        : Notification(NotificationType::peer_transaction)
        , PeerTransaction(input)
      {}

      void
      Notification::print(std::ostream& stream) const
      {
        stream << this->notification_type;
      }

      ConfigurationNotification::ConfigurationNotification(
        elle::json::Object json)
        : Notification(NotificationType::configuration)
        , configuration(json)
      {}

      void
      UserStatusNotification::print(std::ostream& stream) const
      {
        stream << "Notification user " << user_id
               << " went " << (device_status ? "on" : "off")
               << "line on device " << device_id;
      }

      void
      LinkTransactionNotification::print(std::ostream& stream) const
      {
        stream << this->notification_type << ": "
               << *static_cast<LinkTransaction const*>(this);
      }

      PeerReachabilityNotification::PeerReachabilityNotification()
        : Notification(NotificationType::peer_connection_update)
      {}

      static
      std::unique_ptr<Notification>
      notification_from_dict(elle::json::Object const& json)
      {
        ELLE_TRACE("convert json %s to notification", json);
        auto type =
          NotificationType(
            boost::any_cast<int64_t>(json.at("notification_type")));
        switch (type)
        {
          case NotificationType::peer_connection_update:
          {
            auto res = elle::make_unique<PeerReachabilityNotification>();
            res->transaction_id =
              boost::any_cast<std::string>(json.at("transaction_id"));
            res->status =
              boost::any_cast<bool>(json.at("status"));
            if (res->status)
            {
              auto const& endpoints =
                boost::any_cast<elle::json::Object>(json.at("peer_endpoints"));
              for (auto const& elt_:
                     boost::any_cast<elle::json::Array>(endpoints.at("locals")))
              {
                auto const& elt = boost::any_cast<elle::json::Object>(elt_);
                auto ip = boost::any_cast<std::string>(elt.at("ip"));
                auto port = boost::any_cast<int64_t>(elt.at("port"));
                res->endpoints_local.push_back(
                  PeerReachabilityNotification::Endpoint(ip, port));
              }
              for (auto const& elt_:
                boost::any_cast<elle::json::Array>(endpoints.at("externals")))
              {
                auto const& elt = boost::any_cast<elle::json::Object>(elt_);
                auto ip = boost::any_cast<std::string>(elt.at("ip"));
                auto port = boost::any_cast<int64_t>(elt.at("port"));
                res->endpoints_public.push_back(
                  PeerReachabilityNotification::Endpoint(ip, port));
              }
            }
            return std::move(res);
          }
          case NotificationType::configuration:
          {
            return elle::make_unique<ConfigurationNotification>(
              std::move(json));
          }
          case NotificationType::peer_transaction:
          {
            elle::serialization::json::SerializerIn input(json);
            return elle::make_unique<PeerTransactionNotification>(input);
          }
          default:
            ; // Nothing.
        }
        using namespace elle::serialize;
        std::stringstream stream;
        elle::json::write(stream, json);
        auto repr = stream.str();
        auto extractor = from_string<InputJSONArchive>(repr);
        typedef std::unique_ptr<Notification> Ptr;
        switch (type)
        {
          case NotificationType::ping:
            return Ptr(new Notification{extractor});
          case NotificationType::link_transaction:
            return Ptr(new LinkTransactionNotification{extractor});
          case NotificationType::peer_transaction:
            elle::unreachable();
          case NotificationType::peer_connection_update:
            elle::unreachable();
          case NotificationType::new_swagger:
            return Ptr(new NewSwaggerNotification{extractor});
          case NotificationType::deleted_swagger:
            return Ptr(new DeletedSwaggerNotification{extractor});
          case NotificationType::deleted_favorite:
            return Ptr(new DeletedFavoriteNotification{extractor});
          case NotificationType::user_status:
            return Ptr(new UserStatusNotification{extractor});
          case NotificationType::message:
            return Ptr(new MessageNotification{extractor});
          case NotificationType::connection_enabled:
            return Ptr(new ConnectionEnabledNotification{extractor});
            // XXX: Handle at upper levels (?)
          case NotificationType::invalid_credentials:
            return Ptr(new InvalidCredentialsNotification());
          case NotificationType::suicide:
            SUICIDE();
          default:
            throw elle::Exception{elle::sprint("Unknown notification type", type)};
        }
        elle::unreachable();
      }

      //- Implementation --------------------------------------------------------
      struct Client::Impl:
        public elle::Printable
      {
        Client& _client;
        int _reconnected;
        std::shared_ptr<reactor::network::FingerprintedSocket> _socket;
        reactor::Barrier _connected;
        std::string server;
        uint16_t port;
        boost::asio::streambuf request;
        boost::asio::streambuf response;
        boost::system::error_code last_error;
        std::string user_id;
        std::string user_device_id;
        std::string user_session_id;
        Client::ConnectCallback connect_callback;
        Client::ReconnectPokeFailedCallback reconnect_failed_callback;
        std::unique_ptr<reactor::Thread> _connect_callback_thread;
        std::unique_ptr<reactor::Thread> _connect_thread;

        Impl(Client& client,
             Client::ConnectCallback connect_callback,
             Client::ReconnectPokeFailedCallback reconnect_failed_callback):
          _client(client),
          _reconnected(0),
          _socket(),
          _connected(),
          server(),
          port(),
          request{},
          response{},
          connect_callback{connect_callback},
          reconnect_failed_callback{reconnect_failed_callback},
          _ping_period(default_ping_period),
          _ping_timeout(this->_ping_period * 2),
          _ping_signal(),
          _ping_thread(),
          _pong_signal(),
          _pong_thread(),
          _read_thread(),
          _connect_timeout(20_sec),
          _poke_timeout(15_sec)
        {}

        virtual
        ~Impl()
        {
          this->_threads_teardown();
        }

        void
        _threads_setup()
        {
          if (!this->_ping_thread)
          {
            ELLE_TRACE_SCOPE("%s: start ping thread", *this);
            this->_ping_thread.reset(
              new reactor::Thread(
                elle::sprintf("%s ping thread", *this),
                [&] () { this->ping_thread(); }));
          }
          if (!this->_pong_thread)
          {
            ELLE_TRACE_SCOPE("%s: start pong thread", *this);
            this->_pong_thread.reset(
              new reactor::Thread(
                elle::sprintf("%s pong thread", *this),
                [&] () { this->pong_thread(); }));
          }
          if (!this->_read_thread)
          {
            ELLE_TRACE_SCOPE("%s: start read thread", *this);
            this->_read_thread.reset(
              new reactor::Thread(
                elle::sprintf("%s read thread", *this),
                [&] () { this->read_thread(); }));
          }
        }

        // Cleanup and reset all threads.
        //
        // * We might be called from one of the read/ping/pong thread trying to
        //   reconnect, so don't suicide or delete (reset) oneself.
        //
        // * Terminate all threads and then wait for them to prevent one of them
        //   from reentering the teardown processus while we wait on another
        //   one.

        void
        _threads_teardown()
        {
          std::unique_ptr<reactor::Thread> ping_thread;
          std::unique_ptr<reactor::Thread> pong_thread;
          std::unique_ptr<reactor::Thread> read_thread;
          std::unique_ptr<reactor::Thread> connect_callback_thread;
          std::unique_ptr<reactor::Thread> connect_thread;
          reactor::Waitables waited;
          auto current = reactor::scheduler().current();
          auto stop =
            [&] (std::unique_ptr<reactor::Thread>& t)
            {
              return t.get() && t.get() != current;
            };
          if (stop(this->_ping_thread))
            ELLE_DEBUG("%s: terminate ping thread", *this)
            {
              this->_ping_thread->terminate();
              waited.push_back(this->_ping_thread.get());
              ping_thread = std::move(this->_ping_thread);
            }
          if (stop(this->_pong_thread))
            ELLE_DEBUG("%s: terminate pong thread", *this)
            {
              this->_pong_thread->terminate();
              waited.push_back(this->_pong_thread.get());
              pong_thread = std::move(this->_pong_thread);
            }
          if (stop(this->_read_thread))
            ELLE_DEBUG("%s: terminate read thread", *this)
            {
              this->_read_thread->terminate();
              waited.push_back(this->_read_thread.get());
              read_thread = std::move(this->_read_thread);
            }
          if (this->_connect_callback_thread)
            ELLE_DEBUG("%s: terminate connect callback thread", *this)
            {
              this->_connect_callback_thread->terminate();
              waited.push_back(this->_connect_callback_thread.get());
              connect_callback_thread =
                std::move(this->_connect_callback_thread);
            }
            if (this->_connect_thread)
            ELLE_DEBUG("%s: terminate connect callback thread", *this)
            {
              this->_connect_thread->terminate();
              waited.push_back(this->_connect_thread.get());
              connect_thread =
                std::move(this->_connect_thread);
            }
          reactor::wait(waited);
        }

        std::vector<unsigned char> _test_fingerprint;
        void
        _set_test_fingerprint(std::vector<unsigned char> new_fingerprint)
        {
          _test_fingerprint = new_fingerprint;
        }

        std::shared_ptr<reactor::network::FingerprintedSocket>
        _poke(reactor::DurationOpt const& timeout = reactor::DurationOpt())
        {
          std::shared_ptr<reactor::network::FingerprintedSocket> socket;
          ELLE_TRACE("%s: connect to %s:%s", *this, this->server, this->port)
          {
            try
            {
              auto endpoint = reactor::network::resolve_tcp(
                this->server, std::to_string(this->port));
              socket = std::make_shared<reactor::network::FingerprintedSocket>(
                endpoint, this->_test_fingerprint, this->connect_timeout());
              socket->shutdown_asynchronous(true);
            }
            catch (reactor::network::Exception const& e)
            {
              throw Unreachable(
                this->server, this->port,
                elle::sprintf("connection error: %s",
                              elle::exception_string()));
            }
          }
          ELLE_TRACE("%s: poke", *this)
          {
            std::string const poke_msg("ouch");
            elle::json::Object poke_obj;
            poke_obj["poke"] = poke_msg;
            try
            {
              ELLE_DEBUG("%s: send poke message: %s", *this, poke_msg)
                elle::json::write(*socket, poke_obj);
            }
            catch (reactor::network::Exception const& e)
            {
              throw Unreachable(
                this->server, this->port,
                elle::sprintf("network error sending poke: %s",
                              elle::exception_string()));
            }
            elle::json::Json poke_reply_read;
            ELLE_DEBUG("%s: get poke reply", *this)
            {
              try
              {
                reactor::TimeoutGuard guard(
                  timeout ? timeout.get() : this->_poke_timeout);
                poke_reply_read = elle::json::read(*socket);
              }
              catch (reactor::Timeout const&)
              {
                throw Unreachable(
                  this->server, this->port,
                  elle::sprintf("poke timeout after %s", this->_poke_timeout));
              }
              catch (elle::json::ParseError const& e)
              {
                throw Unreachable(
                  this->server, this->port,
                  elle::sprintf("JSON parse error in poke response: %s", e));
              }
              catch (reactor::network::Exception const& e)
              {
                throw Unreachable(
                  this->server, this->port,
                  elle::sprintf("network error reading poke response: %s",
                                elle::exception_string()));
              }
            }
            try
            {
              auto const& poke_reply =
                boost::any_cast<elle::json::Object>(poke_reply_read);
              std::string reply_msg =
                boost::any_cast<std::string>(poke_reply.at("poke"));
              if (reply_msg == poke_msg)
              {
                ELLE_DEBUG("%s: got correct poke reply", *this);
              }
              else
              {
                throw Unreachable(
                  this->server, this->port, "invalid poke reply");
              }
            }
            catch (boost::bad_any_cast const&)
            {
              throw Unreachable(
                this->server, this->port, "invalid poke reply");
            }
          }
          return socket;
        }

        void
        _connect(std::string const& user_id,
                 std::string const& device_id,
                 std::string const& session_id)
        {
          this->user_id = user_id;
          this->user_device_id = device_id;
          this->user_session_id = session_id;
          _connect_thread.reset(
            new reactor::Thread(
              "reconnection",
              [&]
              {
                this->_reconnect(true);
              }
              ));
        }


        void
        _connect()
        {
          auto const connection_timeout = this->connect_timeout();
          try
          {
            ELLE_TRACE_SCOPE("%s: connect to %s:%s",
                           *this, this->server, this->port);
            ELLE_ASSERT(!this->_connected.opened());
            auto socket = this->_poke();

            ELLE_DEBUG_SCOPE("%s: authenticate", *this);
            elle::json::Object request;
            {
              request["user_id"] = this->user_id;
              request["session_id"] = this->user_session_id;
              request["device_id"] = this->user_device_id;
              {
                elle::json::Object version;
                version["major"] = INFINIT_VERSION_MAJOR;
                version["minor"] = INFINIT_VERSION_MINOR;
                version["subminor"] = INFINIT_VERSION_SUBMINOR;
                request["version"] = version;
              }
              request["os"] = elle::system::platform::os_name();
              request["os_version"] =
                elle::system::platform::os_version();
            }
            ELLE_DUMP("%s: authentication request: %s", *this, request);
            elle::json::write(*socket, request);
            ELLE_DEBUG("%s: read authentication response", *this);
            auto notif = read(connection_timeout, socket);
            ELLE_DUMP("%s: authentication response: %s", *this, *notif);
            if (notif->notification_type !=
                NotificationType::connection_enabled)
              throw elle::Error("wrong first notification");
            auto notification =
              elle::cast<ConnectionEnabledNotification>::runtime(notif);
            ELLE_ASSERT(notification.get());
            if (notification->response_code != 200)
              throw elle::Error(notification->response_details);
            ELLE_LOG("%s: connected to %s:%s", *this, this->server, this->port);
            this->_socket = std::move(socket);
            this->_socket->shutdown_asynchronous(false);
            this->_threads_setup();
            this->_connected.open();
          }
          catch (reactor::network::TimeOut const& e)
          {
            throw ConnectionError(
              this->server, this->port,
              elle::sprintf("authentication timeout after %s",
                            connection_timeout));
          }
          catch (reactor::network::Exception const& e)
          {
            throw ConnectionError(
              this->server, this->port,
              elle::sprintf("network error during authentication: %s", e));
          }
          catch (elle::Exception const& e)
          {
            ELLE_ERR("%s: unexpected authentication exception: %s", *this, e);
            throw ConnectionError(
              this->server, this->port,
              elle::sprintf("error during authentication: %s", e));
          }
        }

        void
        _disconnect()
        {
          ELLE_TRACE_SCOPE("%s: disconnect", *this);
          this->_connected.close();
          this->_notifications.close();
          this->_notifications.clear();
          this->_threads_teardown();
          if (this->_connect_callback_thread)
          {
            this->_connect_callback_thread->terminate_now();
            this->_connect_callback_thread.reset();
          }
          this->_socket.reset();
        }

        void
        _reconnect(bool first_time = false)
        {
          // If several threads try to reconnect, just wait for the first one to
          // be done.
          ELLE_TRACE_SCOPE("%s: thread %s reconnect",
                           *this, *reactor::Scheduler::scheduler()->current());
          if (this->_connected.opened())
          {
            // Since we're reconnecting because of network issue anyway, we don't
            // really care about a proper SSL shutdown before reconnecting.
            this->_socket->shutdown_asynchronous(true);
            this->_disconnect();
          }
          if (!first_time)
            this->connect_callback(false);
          while (true)
          {
            try
            {
              this->_connect();
              if (!first_time)
                this->_connect_callback_thread.reset(
                  new reactor::Thread(
                    "reconnection callback",
                    [&]
                    {
                      ELLE_LOG("%s: running reconnection callback", *this);
                      try
                      {
                        this->connect_callback(true);
                      }
                      catch (reactor::Terminate const&)
                      {
                        throw;
                      }
                      catch (...)
                      {
                        ELLE_ERR("%s: connection callback failed: %s",
                                 *this, elle::exception_string());
                        throw;
                      }
                      this->_notifications.open();
                    }));
              break;
            }
            catch (ConnectionError const& e)
            {
              this->connect_callback(false);
              ELLE_WARN("%s: unable to reconnect: %s",
                        *this, elle::exception_string());
            }
            this->reconnect_failed_callback();
            boost::random::mt19937 rng;
            rng.seed(static_cast<unsigned int>(std::time(0)));
            boost::random::uniform_int_distribution<> random(100, 150);
            auto const delay =
              this->_client.reconnection_cooldown() * random(rng) / 100;
            ELLE_LOG("%s: will retry reconnection in %s", *this, delay);
            reactor::sleep(delay);
          }
          ELLE_ASSERT(this->_connected.opened());
          ELLE_LOG("%s: reconnected to trophonius", *this);
          if (!first_time)
            ++this->_reconnected;
        }

        boost::posix_time::time_duration _ping_period;
        boost::posix_time::time_duration _ping_timeout;
        reactor::Signal _ping_signal;
        std::unique_ptr<reactor::Thread> _ping_thread;
        void
        ping_thread()
        {
          ELLE_TRACE_SCOPE("start ping thread");
          static std::string const ping_msg("{\"notification_type\": 208}\n");

          while (true)
          {
            // Wait for the ping signal, or ping_period at most.
            this->_ping_signal.wait(this->_ping_period);

            if (!this->_connected.opened())
            {
              ELLE_DUMP("%s: ping thread: waiting to be connected", *this);
              reactor::wait(this->_connected);
              ELLE_DUMP("%s: ping thread: connected", *this);
            }

            try
            {
              auto socket = this->_socket;
              ELLE_DUMP_SCOPE("send ping to %s", socket->peer());
              socket->write(elle::ConstWeakBuffer(ping_msg));
            }
            catch (elle::Exception const&)
            {
              ELLE_WARN("couldn't send ping to tropho: %s",
                        elle::exception_string());
              this->_reconnect();
            }
          }
        }

        reactor::Signal _pong_signal;
        std::unique_ptr<reactor::Thread> _pong_thread;
        void
        pong_thread()
        {
          ELLE_TRACE_SCOPE("start pong thread");
          while (true)
          {
            if (!this->_connected.opened())
            {
              ELLE_DUMP("%s: pong thread: waiting to be connected", *this);
              reactor::wait(this->_connected);
              ELLE_DUMP("%s: pong thread: connected", *this);
            }

            if (!this->_pong_signal.wait(this->_ping_timeout))
            {
              ELLE_WARN("%s: didn't receive ping from tropho in %s",
                        *this, this->_ping_timeout);
              this->_reconnect();
            }
          }
        }

        std::unique_ptr<Notification>
        read(reactor::Duration timeout,
             std::shared_ptr<reactor::network::Socket> socket)
        {
          ELLE_DUMP_SCOPE("%s: read message", *this);
          auto buffer = socket->read_until("\n", timeout);
          ELLE_TRACE("%s: got message: %f", *this, buffer);
          elle::InputStreamBuffer<elle::Buffer> streambuffer(buffer);
          std::istream input(&streambuffer);
          auto json = boost::any_cast<elle::json::Object>(
            elle::json::read(input));
          return notification_from_dict(json);
        }

        std::unique_ptr<reactor::Thread> _read_thread;
        void
        read_thread()
        {
          ELLE_TRACE_SCOPE("start read thread");
          while (true)
          {
            ELLE_DUMP("%s: read thread: waiting to be connected", *this);
            reactor::wait(this->_connected);
            ELLE_DUMP("%s: read thread: connected", *this);
            auto notif = std::unique_ptr<Notification>();
            // The read timeout should never be less than the ping timeout.
            reactor::Duration read_timeout = this->_ping_timeout * 3;
            try
            {
              notif = this->read(read_timeout, this->_socket);
            }
            catch (reactor::network::TimeOut const& e)
            {
              ELLE_WARN("%s: timeout after %s while reading", *this,
                        read_timeout);
              this->_reconnect();
              continue;
            }
            catch (elle::Exception const& e)
            {
              ELLE_WARN("%s: error while reading: %s", *this, e.what());
              this->_reconnect();
              continue;
            }
            ELLE_ASSERT(notif != nullptr);
            if (notif->notification_type == NotificationType::ping)
            {
              ELLE_DUMP("%s: ping received", *this);
              this->_pong_signal.signal();
              continue;
            }
            else
            {
              ELLE_DUMP("%s: enqueue notification", *this);
              this->_notifications.put(std::move(notif));
            }
          }
        }

        std::unique_ptr<Notification>
        poll()
        {
          // Check that there actually is a notification availble: we may be
          // awaken because a notification was received, but a ping timeout
          // disconnects and empties the notifications list before we get a
          // chance to execute.
          auto res = this->_notifications.get();
          ELLE_TRACE("%s: handle new notification", *this);
          return std::move(res);
        }

        /*----------.
        | Printable |
        `----------*/

        void
        print(std::ostream& stream) const override
        {
          stream << "trophonius::Client("
                 << "\"" << this->user_id << "\", "
                 << "\"" << this->user_device_id << "\")";
        }

        friend class Client;
        typedef reactor::Channel<std::unique_ptr<Notification>> Notifications;
        ELLE_ATTRIBUTE(Notifications, notifications);
        ELLE_ATTRIBUTE_RW(reactor::Duration, connect_timeout);
        ELLE_ATTRIBUTE_RW(reactor::Duration, poke_timeout);
      };

      Client::Client(ConnectCallback connect_callback,
                     ReconnectPokeFailedCallback reconnect_failed_callback,
                     std::vector<unsigned char> server_fingerprint,
                     boost::posix_time::time_duration reconnection_cooldown)
        : _impl(new Impl(*this, connect_callback, reconnect_failed_callback))
        , _ping_period(default_ping_period)
        , _reconnection_cooldown(reconnection_cooldown)
      {
        ELLE_ASSERT(connect_callback != nullptr);
        this->_impl->_set_test_fingerprint(server_fingerprint);
      }

      Client::Client(std::string host,
                     int port,
                     ConnectCallback connect_callback,
                     ReconnectPokeFailedCallback reconnect_failed_callback,
                     std::vector<unsigned char> server_fingerprint,
                     boost::posix_time::time_duration reconnection_cooldown):
        Client(std::move(connect_callback),
               std::move(reconnect_failed_callback),
               std::move(server_fingerprint),
               reconnection_cooldown)
      {
        this->server(std::move(host), port);
      }

      reactor::Barrier const&
      Client::connected() const
      {
        return this->_impl->_connected;
      }

      reactor::Barrier&
      Client::connected()
      {
        return this->_impl->_connected;
      }

      void
      Client::ping_period(boost::posix_time::time_duration const& value)
      {
        this->_impl->_ping_period = value;
        this->_impl->_ping_timeout = value * 2;
        // Reping immediately.
        this->_impl->_ping_signal.signal();
      }

      boost::posix_time::time_duration const&
      Client::ping_period() const
      {
        return this->_impl->_ping_period;
      }

      Client::~Client()
      {
        ELLE_TRACE_SCOPE("%s: terminate", *this);
        delete _impl;
        _impl = nullptr;
      }

      int
      Client::reconnected() const
      {
        return this->_impl->_reconnected;
      }

      void
      Client::poke_timeout(reactor::Duration const& timeout)
      {
        this->_impl->poke_timeout(timeout);
      }

      void
      Client::connect_timeout(reactor::Duration const& timeout)
      {
        this->_impl->connect_timeout(timeout);
      }

      void
      Client::connect(std::string const& _id,
                      std::string const& device_id,
                      std::string const& session_id)
      {
        ELLE_TRACE_SCOPE(
          "%s: connecting with id \"%s\", session \"%s\", device_id \"%s\"",
          *this, _id, session_id, device_id);
        this->_impl->_connect(_id, device_id, session_id);
        this->_impl->_notifications.open();
      }

      void
      Client::server(std::string const& host, int port)
      {
        ELLE_TRACE_SCOPE("%s: set server (%s:%s)", *this, host, port);
        this->_impl->server = host;
        this->_impl->port = port;
      }

      void
      Client::disconnect()
      {
        this->_impl->_disconnect();
      }

      std::unique_ptr<Notification>
      Client::poll()
      {
        return this->_impl->poll();
      }

      void
      Client::print(std::ostream& stream) const
      {
        stream << *this->_impl;
      }

      /*-----------.
      | Exceptions |
      `-----------*/

      ConnectionError::ConnectionError(
        std::string host, int port, std::string message)
        : Super(elle::sprintf("unable connect to trophonius on %s:%s: %s",
                              host, port, message))
        , _host(std::move(host))
        , _port(std::move(port))
      {}

      Unreachable::Unreachable(std::string host, int port, std::string message)
        : Super(std::move(host), std::move(port),
                elle::sprintf("unreachable: %s", message))
      {}
    }
  }
}
