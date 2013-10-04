#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <iostream>

#include <elle/Buffer.hh>
#include <elle/assert.hh>
#include <elle/finally.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/format/json/Parser.hh>
#include <elle/log.hh>
#include <elle/memory.hh>
#include <elle/print.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/NamedValue.hh>
#include <elle/serialize/Serializer.hh>
#include <elle/serialize/extract.hh>

#include <reactor/Barrier.hh>
#include <reactor/exception.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>
#include <reactor/signal.hh>
#include <reactor/thread.hh>


#include <plasma/plasma.hh>
#include <plasma/trophonius/Client.hh>

ELLE_LOG_COMPONENT("infinit.plasma.trophonius.Client");

/*-------------------------.
| Notification serializers |
`-------------------------*/
ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::Notification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::Notification, ar, value, version)
{
  (void)version;
  ar & named("notification_type", value.notification_type);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::ConnectionEnabledNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::ConnectionEnabledNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & named("response_code", value.response_code);
  ar & named("response_details", value.response_details);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::NewSwaggerNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::NewSwaggerNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & named("user_id", value.user_id);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::UserStatusNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::UserStatusNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & named("user_id", value.user_id);
  ar & named("status", value.status);
  ar & named("device_id", value.device_id);
  ar & named("device_status", value.device_status);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::TransactionNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::TransactionNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & base_class<plasma::Transaction>(value);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::PeerConnectionUpdateNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::PeerConnectionUpdateNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & named("transaction_id", value.transaction_id);
  ar & named("status", value.status);
  ar & named("devices", value. devices);
}


ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::NetworkUpdateNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::NetworkUpdateNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & named("network_id", value.network_id);
  ar & named("what", value.what);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::MessageNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::MessageNotification,
                      ar,
                      value,
                      version)
{
  (void)version;
  ar & base_class<plasma::trophonius::Notification>(value);
  ar & named("sender_id", value.sender_id);
  ar & named("message", value.message);
}

namespace plasma
{
  namespace trophonius
  {
    boost::posix_time::time_duration const default_ping_period(
      boost::posix_time::seconds(30));

    /*--------------.
    | Notifications |
    `--------------*/
    void
    Notification::print(std::ostream& stream) const
    {
      stream << this->notification_type;
    }

    void
    TransactionNotification::print(std::ostream& stream) const
    {
      stream << this->notification_type << ": "
             << *static_cast<Transaction const*>(this);
    }

    std::unique_ptr<Notification>
    notification_from_dict(json::Dictionary const& dict)
    {
      ELLE_TRACE("convert json %s to Notification instance", dict.repr());
      NotificationType type = dict["notification_type"].as<NotificationType>();
      using namespace elle::serialize;
      auto extractor = from_string<InputJSONArchive>(dict.repr());
      typedef std::unique_ptr<Notification> Ptr;
      switch (type)
      {
      case NotificationType::ping:
        return Ptr(new Notification{extractor});
      case NotificationType::transaction:
        return Ptr(new TransactionNotification{extractor});
      case NotificationType::peer_connection_update:
        return Ptr(new PeerConnectionUpdateNotification{extractor});
      case NotificationType::new_swagger:
        return Ptr(new NewSwaggerNotification{extractor});
      case NotificationType::user_status:
        return Ptr(new UserStatusNotification{extractor});
      case NotificationType::message:
        return Ptr(new MessageNotification{extractor});
      case NotificationType::network_update:
        return Ptr(new NetworkUpdateNotification{extractor});
      case NotificationType::connection_enabled:
        return Ptr(new ConnectionEnabledNotification{extractor});
      // XXX: Handle at upper levels (?)
      case NotificationType::suicide:
        kill(getpid(), SIGKILL);
      default:
        throw elle::Exception{elle::sprint("Unknown notification type", type)};
      }
      elle::unreachable();
    }

    //- Implementation --------------------------------------------------------
    struct Client::Impl:
      public elle::Printable
    {
      int _reconnected;
      std::shared_ptr<reactor::network::TCPSocket> _socket;
      reactor::Barrier _connected;
      bool _synchronized;
      reactor::Barrier _pollable;
      std::string server;
      uint16_t port;
      boost::asio::streambuf request;
      boost::asio::streambuf response;
      boost::system::error_code last_error;
      std::string user_id;
      std::string user_token;
      std::string user_device_id;
      Client::ConnectCallback connect_callback;
      std::unique_ptr<reactor::Thread> _connect_callback_thread;
      std::exception_ptr _poll_exception;
      elle::Buffer _buffer;

      Impl(std::string const& server,
           uint16_t port,
           Client::ConnectCallback connect_callback):
        _reconnected{0},
        _socket(),
        _connected(),
        _synchronized(false),
        _pollable(),
        server(server),
        port(port),
        request{},
        response{},
        connect_callback{connect_callback},
        _poll_exception(),
        _buffer{},
        _ping_period(default_ping_period),
        _ping_timeout(this->_ping_period * 2),
        _ping_signal(),
        _ping_thread(*reactor::Scheduler::scheduler(),
                     elle::sprintf("%s ping thread", *this),
                     [&] () { this->ping_thread(); }),
        _pong_signal(),
        _pong_thread(*reactor::Scheduler::scheduler(),
                     elle::sprintf("%s pong thread", *this),
                     [&] () { this->pong_thread(); }),
        _read_thread(*reactor::Scheduler::scheduler(),
                     elle::sprintf("%s read thread", *this),
                     [&] () { this->read_thread(); })
      {
        this->_buffer.capacity(1024);
      }

      ~Impl()
      {
        this->_ping_thread.terminate_now();
        this->_pong_thread.terminate_now();
        this->_read_thread.terminate_now();
        if (this->_connect_callback_thread)
          this->_connect_callback_thread->terminate_now();
      }

      void
      _connect(bool user_action = false)
      {
        ELLE_DEBUG_SCOPE("%s: connecting trophonius client to %s:%s",
                         *this, this->server, this->port);

        this->_poll_exception = nullptr;
        this->_pollable.close();

        while (!this->_connected.opened())
        {

          try
          {
            if (this->user_id.empty() ||
                this->user_token.empty() ||
                this->user_device_id.empty())
            {
              throw elle::Exception("some of the attributes are empty");
            }

            ELLE_DEBUG("%s: establishing connection", *this);
            auto socket = std::make_shared<reactor::network::TCPSocket>(
              *reactor::Scheduler::scheduler(), this->server, this->port);

            this->_socket = socket;
            ELLE_DEBUG("%s: socket connected", *this);
            // XXX: restore this by exposing the API in reactor's TCP socket.
            // _impl->socket.set_option(boost::asio::socket_base::keep_alive{true});

            // Do not inherit file descriptor when forking.
            // XXX: What for ? Needed ?
            // ::fcntl(this->_socket.native_handle(), F_SETFD, 1);

            json::Dictionary connection_request{
              std::map<std::string, std::string>{
                {"user_id", this->user_id},
                {"token", this->user_token},
                {"device_id", this->user_device_id},
                  }};

            // May raise an exception.
            std::stringstream request;
            elle::serialize::OutputJSONArchive(request, connection_request);

            // Add '\n' to request.
            request << std::endl;

            ELLE_DEBUG("%s: identification", *this);
            socket->write(reactor::network::Buffer(request.str()));

            ELLE_DEBUG("%s: wait for confirmation", *this);
            auto notif = read();

            ELLE_ASSERT(notif != nullptr);
            ELLE_DEBUG("%s: read: %s", *this, *notif);

            if (notif->notification_type != NotificationType::connection_enabled)
            {
              throw elle::Exception("wrong first notification");
            }

            ELLE_ASSERT(dynamic_cast<ConnectionEnabledNotification*>(notif.get()));

            auto notification = std::unique_ptr<ConnectionEnabledNotification>(
              static_cast<ConnectionEnabledNotification*>(notif.release()));

            if (notification->response_code != 200)
            {
              throw elle::Exception(notification->response_details);
            }

            ELLE_LOG("%s: connected", *this);
            this->_connected.open();
          }
          catch (reactor::network::Exception const& e)
          {
            // In that case, we encoutered connection problem, nothing is lost!
            // Let's try again!
            ELLE_WARN("%s: connection failed, wait before retrying: %s",
                      *this, e.what());

            reactor::sleep(1_sec);
          }
          catch (elle::Exception const& e)
          {
            // If an error occured, the connection to trophonius is broken and
            // requiere a new data (user_id, token, ...).
            this->_disconnect();

            ELLE_ERR("%s: failure to connect: %s", *this, e.what());
            this->_poll_exception = std::make_exception_ptr(e);
            // Wake up the poll thread to throw this exception.
            this->_pollable.open();
            // If the action has been initiated by the user, we can throw him
            // the exception.
            if (user_action)
              throw;
          }
        }
      }

      void
      _disconnect()
      {
        this->_connected.close();
        this->_synchronized = false;
        this->_pollable.close();
        while (!this->_notifications.empty())
          this->_notifications.pop();
        if (this->_connect_callback_thread)
        {
          this->_connect_callback_thread->terminate_now();
          this->_connect_callback_thread.reset();
        }
        this->_buffer.reset();
        if (this->_socket)
        {
          this->_socket->close();
          this->_socket.reset();
        }
      }

      void
      _reconnect(bool require_connected = true)
      {
        // If several threads try to reconnect, just wait for the first one to
        // be done.
        if (require_connected && !this->_connected.opened())
        {
          this->_connected.wait();
          return;
        }

        this->_disconnect();
        this->connect_callback(false);
        while (true)
        {
          try
          {
            this->_connect();
            auto& sched = *reactor::Scheduler::scheduler();
            this->_connect_callback_thread.reset(
              new reactor::Thread(
                sched,
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
                  this->_synchronized = true;
                  if (!this->_notifications.empty())
                  {
                    ELLE_DEBUG("%s: resynchronized, "
                               "resume handling notifications", *this);
                    this->_pollable.open();
                  }
                }));
            break;
          }
          catch (elle::Exception const&)
          {
            ELLE_WARN("%s: unable to reconnect to trophonius: %s",
                      *this, elle::exception_string());
            reactor::Scheduler::scheduler()->current()->sleep(10_sec);
          }
        }
        ELLE_LOG("%s: reconnected to trophonius", *this);
        this->_reconnected++;
      }

      boost::posix_time::time_duration _ping_period;
      boost::posix_time::time_duration _ping_timeout;
      reactor::Signal _ping_signal;
      reactor::Thread _ping_thread;
      void
      ping_thread()
      {
        ELLE_TRACE_SCOPE("start ping thread");
        static std::string const ping_msg("{\"notification_type\": 208}\n");

        while (true)
        {
          // Wait for the ping signal, or ping_period at most.
          this->_ping_signal.wait(this->_ping_period);

          ELLE_DEBUG("%s: ping thread: waiting to be connected", *this);
          reactor::Scheduler::scheduler()->current()->wait(this->_connected);
          ELLE_DEBUG("%s: ping thread: connected", *this);

          auto socket = this->_socket;
          try
          {
            ELLE_DEBUG_SCOPE("send ping to %s", socket->remote_locus());
            socket->write(reactor::network::Buffer(ping_msg));
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
      reactor::Thread _pong_thread;
      void
      pong_thread()
      {
        ELLE_TRACE_SCOPE("start pong thread");
        while (true)
        {
          ELLE_DEBUG("%s: pong thread: waiting to be connected", *this);
          reactor::Scheduler::scheduler()->current()->wait(this->_connected);
          ELLE_DEBUG("%s: pong thread: connected", *this);
          if (!this->_pong_signal.wait(this->_ping_timeout))
          {
            ELLE_WARN("%s: didn't receive ping from tropho in %s",
                      *this, this->_ping_timeout);
            this->_reconnect();
          }
        }
      }

      std::unique_ptr<Notification>
      read()
      {
        auto socket = this->_socket;

        this->_buffer.size(0);
        ELLE_DEBUG("%s: reading message", *this);
        size_t idx = 0;
        while (true)
        {
          socket->getline(((char*)this->_buffer.mutable_contents()) + idx,
                          this->_buffer.capacity() - idx, '\n');
          if (!socket->fail())
          {
            this->_buffer.size(idx + socket->gcount());
            break;
          }
          idx = this->_buffer.capacity() - 1;
          this->_buffer.capacity(this->_buffer.capacity() * 2);
          socket->clear();
        }
        if (this->_buffer.size() == 0)
        {
          ELLE_ERR("Empty line read from tropho: bad:%s fail:%s eof:%s gcount:%s",
                   socket->bad(), socket->fail(), socket->eof(), socket->gcount());
          // XXX getline should not return an empty buffer, but throw a
          // massive exception.
          throw elle::Exception{"read an empty buffer"};
        }

        ELLE_TRACE_SCOPE("%s: got message", *this);
        // XXX: Shitty JSON parser seeks.
        // elle::InputStreamBuffer<elle::Buffer> streambuffer(buffer);
        // std::istream input(&streambuffer);
        std::string buffer_str(reinterpret_cast<char const*>(
                                 this->_buffer.contents()),
                               this->_buffer.size());
        std::stringstream input(buffer_str);
        ELLE_DUMP("%s: contents: %s", *this, buffer_str);
        return notification_from_dict(json::parse(input)->as_dictionary());
      }

      std::queue<std::unique_ptr<Notification>> _notifications;
      reactor::Thread _read_thread;
      void
      read_thread()
      {
        ELLE_TRACE_SCOPE("start read thread");
        while (true)
        {
          auto notif = std::unique_ptr<Notification>();

          ELLE_DEBUG("%s: read thread: waiting to be connected", *this);
          reactor::Scheduler::scheduler()->current()->wait(this->_connected);
          ELLE_DEBUG("%s: read thread: connected", *this);

          try
          {
            notif = this->read();
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
            ELLE_DEBUG("%s: ping received", *this);
            this->_pong_signal.signal();
            continue;
          }
          else
          {
            this->_notifications.push(std::move(notif));
            if (this->_synchronized)
              this->_pollable.open();
          }
        }
      }

      std::unique_ptr<Notification>
      poll()
      {
        this->_pollable.wait();

        if (this->_poll_exception)
          std::rethrow_exception(this->_poll_exception);

        ELLE_TRACE("%s new notification", *this);
        ELLE_ASSERT(!this->_notifications.empty());
        std::unique_ptr<Notification> res(
          this->_notifications.front().release());
        this->_notifications.pop();
        if (this->_notifications.empty())
          this->_pollable.close();
        return std::move(res);
      }

      /*----------.
      | Printable |
      `----------*/

      void
      print(std::ostream& stream) const override
      {
        stream << "trophonius::Client("
               << this->user_id << ", "
               << this->user_device_id << ")";
      }
    };

    Client::Client(std::string const& server,
                   uint16_t port,
                   ConnectCallback connect_callback):
      _impl{new Impl{server, port, connect_callback}},
      _ping_period(default_ping_period)
    {
      ELLE_ASSERT(connect_callback != nullptr);
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
    {}

    int
    Client::reconnected() const
    {
      return this->_impl->_reconnected;
    }

    bool
    Client::connect(std::string const& _id,
                    std::string const& token,
                    std::string const& device_id)
    {
      this->_impl->user_id = _id;
      this->_impl->user_token = token;
      this->_impl->user_device_id = device_id;
      this->_impl->_connect(true);
      ELLE_LOG("%s: connected to trophonius", *this);
      this->_impl->_synchronized = true;
      return true;
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

    std::ostream&
    operator <<(std::ostream& out,
                NotificationType t)
    {
      switch (t)
      {
#define NOTIFICATION_TYPE(name, value)             \
        case NotificationType::name:           \
          out << #name;                            \
          break;
#include <oracle/disciples/meta/src/meta/notification_type.hh.inc>
#undef NOTIFICATION_TYPE
      }

      return out;
    }

    std::ostream&
    operator <<(std::ostream& out,
                NetworkUpdate n)
    {
      switch (n)
      {
#define NETWORK_UPDATE(name, value)             \
        case NetworkUpdate::name:           \
          out << #name;                            \
          break;
#include <oracle/disciples/meta/src/meta/resources/network_update.hh.inc>
#undef NETWORK_UPDATE
      }

      return out;
    }

    void
    Client::print(std::ostream& stream) const
    {
      stream << *this->_impl;
    }
  }
}
