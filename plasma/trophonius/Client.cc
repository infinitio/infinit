#include "Client.hh"

#include <plasma/plasma.hh>

#include <elle/assert.hh>
#include <elle/log.hh>
#include <elle/print.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/serialize/extract.hh>
#include <elle/format/json/Dictionary.hxx>
#include <elle/format/json/Parser.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/Serializer.hh>
#include <elle/serialize/NamedValue.hh>

#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <iostream>
#include <fstream>

#include <fcntl.h>

ELLE_LOG_COMPONENT("infinit.plasma.trophonius.Client");

//- Notification serializers --------------------------------------------------

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::Notification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::Notification, ar, value, version)
{
  (void)version;
  ar & named("notification_type", value.notification_type);
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
    Notification::~Notification()
    {}

    void
    Notification::print(std::ostream& stream) const
    {
      stream << this->notification_type;
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
      case NotificationType::new_swagger:
        return Ptr(new NewSwaggerNotification{extractor});
      case NotificationType::user_status:
        return Ptr(new UserStatusNotification{extractor});
      case NotificationType::message:
        return Ptr(new MessageNotification{extractor});
      case NotificationType::network_update:
        return Ptr(new NetworkUpdateNotification{extractor});
      case NotificationType::connection_enabled:
        return Ptr(new Notification{extractor});
      default:
        throw elle::Exception{elle::sprint("Unknown notification type", type)};
      }
      elle::unreachable();
    }

    //- Implementation --------------------------------------------------------
    struct Client::Impl
    {
      boost::asio::io_service io_service;
      boost::asio::ip::tcp::socket socket;
      boost::asio::deadline_timer connection_checker;
      boost::asio::deadline_timer ping_timer;
      bool connected;
      bool ping_received;
      std::string server;
      uint16_t port;
      boost::asio::streambuf request;
      boost::asio::streambuf response;
      boost::system::error_code last_error;
      std::string user_id;
      std::string user_token;
      std::string user_device_id;
      std::function<void()> connect_callback;

      Impl(std::string const& server,
           uint16_t port,
           std::function<void()> connect_callback):
        io_service{},
        socket{io_service},
        connection_checker{io_service},
        ping_timer{io_service},
        connected{false},
        ping_received{false},
        server{server},
        port{port},
        request{},
        response{},
        connect_callback{connect_callback}
      {}
    };

    Client::Client(std::string const& server,
                   uint16_t port,
                   std::function<void()> connect_callback):
      _impl{new Impl{server, port, connect_callback}}
    {
      ELLE_ASSERT(connect_callback != nullptr);
    }

    Client::~Client()
    {}

    static char const* ping_msg = "{\"notification_type\": 208}\n";

    void
    Client::_disconnect()
    {
      _impl->connected = false;
      _impl->socket.close();
    }

    void
    Client::_check_connection(boost::system::error_code const& err)
    {
      if (err)
      {
        if (err.value() != boost::asio::error::operation_aborted)
        {
          ELLE_WARN("timer failed in %s (%s), stopping connection checks",
                    __func__, err);
        }
        return;
      }

      if (_impl->connected == false || _impl->ping_received == false)
      {
        try
        {
          ELLE_TRACE("no message from Trophonius for too long.");
          ELLE_TRACE("trying to reconnect");
          this->_disconnect();
          this->connect(_impl->user_id,
                        _impl->user_token,
                        _impl->user_device_id);
          _impl->last_error = boost::system::error_code{};
          ELLE_TRACE("reconnected to Trophonius successfully");
        }
        catch (std::exception const&)
        {
          ELLE_WARN("Couldn't reconnect to tropho: %s",
                    elle::exception_string());
        }
      }
      this->_restart_connection_check_timer();
    }

    void
    Client::_on_ping_sent(boost::system::error_code const& err,
                          size_t const /*bytes_transferred*/)
    {
      if (err)
      {
        this->_disconnect();
        this->connect(_impl->user_id, _impl->user_token, _impl->user_device_id);
        ELLE_WARN("timer failed in %s (%s), stopping connection checks",
                  __func__, err.message());
      }
    }

    void
    Client::_send_ping(boost::system::error_code const& err)
    {
      if (err)
      {
        if (err.value() != boost::asio::error::operation_aborted)
        {
          ELLE_WARN("timer failed (%s), stopping connection checks", err);
        }
        return;
      }

      try
      {
        ELLE_DEBUG("send ping to %s", _impl->socket.remote_endpoint());
        boost::asio::async_write(
          _impl->socket,
          boost::asio::buffer(ping_msg, strlen(ping_msg)),
          std::bind(
            &Client::_on_ping_sent, this,
            std::placeholders::_1, std::placeholders::_2
          )
        );
      }
      catch (std::exception const&)
      {
        ELLE_WARN("couldn't send ping to tropho: %s",
                  elle::exception_string());
        this->_impl->connected = false;
      }
      // Ensure that we try to send a ping every x seconds
      this->_restart_ping_timer();
    }

    void
    Client::_restart_ping_timer()
    {
      _impl->ping_timer.expires_from_now(
        boost::posix_time::seconds(30));

      _impl->ping_timer.async_wait(
        std::bind(&Client::_send_ping, this, std::placeholders::_1));
    }

    void
    Client::_restart_connection_check_timer()
    {
      _impl->connection_checker.expires_from_now(
        boost::posix_time::seconds(60));

      _impl->connection_checker.async_wait(
          std::bind(&Client::_check_connection, this, std::placeholders::_1));
    }

    void
    Client::_connect()
    {
      if (_impl->connected)
        return;

      typedef boost::asio::ip::tcp tcp;
      // Resolve the host name into an IP address.
      tcp::resolver resolver(_impl->io_service);
      tcp::resolver::query query(_impl->server, elle::sprint(_impl->port));
      tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

      ELLE_DEBUG("%s: connecting trophonius client to %s:%s",
                 *this, _impl->server, _impl->port);
      if (_impl->socket.is_open())
      {
        _impl->socket.close();
        _impl->socket.open(boost::asio::ip::tcp::v4());
      }

      // Start connect operation.
      _impl->socket.connect(*endpoint_iterator);
      //_impl->socket.non_blocking(true);
      _impl->socket.set_option(boost::asio::socket_base::keep_alive{true});
      _impl->connected = true;

      // Do not inherit file descriptor when forking.
      ::fcntl(_impl->socket.native_handle(), F_SETFD, 1);
    }

    void Client::_on_read_socket(boost::system::error_code const& err,
                                 size_t bytes_transferred)
    {
      if (err || bytes_transferred == 0)
      {
        _impl->connected = false;
        if (err == boost::asio::error::eof ||
            err == boost::asio::error::connection_reset)
        {
          ELLE_TRACE("%s: disconnected from Trophonius, trying to reconnect",
                     *this);
          this->connect(_impl->user_id,
                        _impl->user_token,
                        _impl->user_device_id);
        }
        else if (err)
        {
          ELLE_WARN("%s: something went wrong while reading from socket: %s",
                    *this, err.message());
          _impl->last_error = err;
        }
        return;
      }

      ELLE_DEBUG("%s: read %s bytes from the socket (%s available)",
                 *this,
                 bytes_transferred,
                 _impl->response.in_avail());

      // Bind stream to response.
      std::istream is(&(_impl->response));

      // Transfer socket stream to stringstream that ensure there are no
      // encoding troubles (and make the stream human readable).
      std::unique_ptr<char[]> data{new char[bytes_transferred]};
      is.read(data.get(), bytes_transferred);
      std::string msg{data.get(), bytes_transferred};
      ELLE_DEBUG("%s: got message: %s", *this, msg);
      try
      {
        auto notif = notification_from_dict(json::parse(msg)->as_dictionary());
        // While there is no reason to forward the ping notification to the user
        // this notification is 'ignored', meaning that it's not pushed into the
        // notification queue.
        // If we want a behavior on it, just remove that condition.
        if (notif->notification_type == NotificationType::ping)
          _impl->ping_received = true;
        else
          this->_notifications.emplace(notif.release());
      }
      catch (std::exception const&)
      {
        ELLE_WARN("%s: couldn't handle %s: %s",
                  *this, msg, elle::exception_string());
      }
      this->_read_socket();
    }

    void
    Client::_read_socket()
    {
      boost::asio::async_read_until(
        _impl->socket, _impl->response, "\n",
        std::bind(
          &Client::_on_read_socket, this,
          std::placeholders::_1, std::placeholders::_2
        )
      );
    }

    bool
    Client::connect(std::string const& _id,
                    std::string const& token,
                    std::string const& device_id)
    {
      _impl->user_id = _id;
      _impl->user_token = token;
      _impl->user_device_id = device_id;
      this->_connect();

      json::Dictionary connection_request{
        std::map<std::string, std::string>{
          {"user_id", _id},
          {"token", token},
          {"device_id", device_id},
      }};

      std::ostream request_stream(&_impl->request);

      // May raise an exception.
      elle::serialize::OutputJSONArchive(request_stream, connection_request);

      // Add '\n' to request.
      request_stream << std::endl;

      boost::system::error_code err;

      boost::asio::write(
        _impl->socket,
        _impl->request,
        err
      );

      if (err)
        throw elle::HTTPException{
          elle::ResponseCode::error, "Writing socket error"};

      this->_read_socket();
      this->_restart_ping_timer();
      this->_restart_connection_check_timer();
      _impl->connect_callback();
      return true;
    }

    std::unique_ptr<Notification>
    Client::poll()
    {
      // Poll while something has to be done
      if (size_t count = _impl->io_service.poll())
      {
        ELLE_DEBUG("%s: polling io service has triggered %s events",
                   *this, count);
      }

      std::unique_ptr<Notification> ret;

      if (!_notifications.empty())
      {
        ELLE_DEBUG("%s: Pop notification dictionnary to be handled.", *this);

        // Fill dictionary.
        ret.reset(_notifications.front().release());
        _notifications.pop();
      }

      return ret;
    }

    bool
    Client::has_notification(void)
    {
      return !(_notifications.empty());
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
      stream << "tropho::Client("
             << this->_impl->user_id << ", "
             << this->_impl->user_device_id << ")";
    }
  }
}
