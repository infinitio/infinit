#include <elle/log.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/format/json/Dictionary.hxx>
#include <elle/format/json/Parser.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/Serializer.hh>
#include <elle/serialize/NamedValue.hh>

#include <surface/gap/gap.h>

#include "Client.hh"

#include <iostream>
#include <fstream>

#include <elle/print.hh>
#include <elle/assert.hh>

#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <fcntl.h>

ELLE_LOG_COMPONENT("infinit.plasma.trophonius.Client");

//- Notification serializers --------------------------------------------------

#define XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE()      \
  int* n = (int*) &value;                                   \
  ar & named("notification_type", *n)                       \
  /**/

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::Notification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::Notification, ar, value, version)
{
  enforce(version == 0);

  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::UserStatusNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::UserStatusNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("user_id", value.user_id);
  ar & named("status", value.status);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::TransactionNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::TransactionNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("transaction", value.transaction);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::TransactionStatusNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::TransactionStatusNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("transaction_id", value.transaction_id);
  ar & named("status", value.status);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::NetworkUpdateNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::NetworkUpdateNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("network_id", value.network_id);
  ar & named("what", value.what);
}

ELLE_SERIALIZE_NO_FORMAT(plasma::trophonius::MessageNotification);
ELLE_SERIALIZE_SIMPLE(plasma::trophonius::MessageNotification, ar, value, version)
{
  enforce(version == 0);

  //ar & base_class<plasma::trophonius::Notification>(value);
  XXX_UGLY_SERIALIZATION_FOR_NOTIFICATION_TYPE();
  ar & named("sender_id", value.sender_id);
  ar & named("message", value.message);
}

namespace plasma
{
  namespace trophonius
  {

    //- Implementation --------------------------------------------------------
    struct Client::Impl
    {
      boost::asio::io_service       io_service;
      boost::asio::ip::tcp::socket  socket;
      boost::asio::deadline_timer   connection_checker;
      bool                          connected;
      std::string                   server;
      uint16_t                      port;
      bool                          check_errors;
      boost::asio::streambuf        request;
      boost::asio::streambuf        response;
      boost::system::error_code     last_error;
      std::string                   user_id;
      std::string                   user_token;
      std::string                   user_device_id;

      Impl(std::string const& server,
           uint16_t port,
           bool check_errors)
        : io_service{}
        , socket{io_service}
        , connection_checker{io_service}
        , connected{false}
        , server{server}
        , port{port}
        , check_errors{check_errors}
        , request{}  // Use once to initiate connection.
        , response{}
      {}
    };

    Client::Client(std::string const& server,
                   uint16_t port,
                   bool check_errors)
      : _impl{new Impl{server, port, check_errors}}
    {
      _impl->connection_checker.expires_from_now(
        boost::posix_time::seconds(10)
      );
      _impl->connection_checker.async_wait(
          std::bind(&Client::_check_connection, this, std::placeholders::_1)
      );
    }

    Client::~Client()
    {
      delete _impl;
      _impl = nullptr;
    }

    void
    Client::_check_connection(boost::system::error_code const& err)
    {
      if (err)
      {
        ELLE_WARN("timer failed, stopping connection checks");
        return;
      }

      static char const* ping = "PING";

      boost::asio::async_write(
        _impl->socket,
        boost::asio::buffer(ping, 4),
        std::bind(
          &Client::_on_write_check, this,
          std::placeholders::_1, std::placeholders::_2
        )
      );
      if (_impl->connected == false)
      {
        try
        {
          ELLE_DEBUG("trying to reconnect to tropho now");
          this->connect(_impl->user_id,
                        _impl->user_token,
                        _impl->user_device_id);
          _impl->last_error = boost::system::error_code{};
          ELLE_DEBUG("reconnect to tropho successfully");
        }
        catch (std::exception const& e)
        {
          ELLE_WARN("Couldn't reconnect to tropho: %s", e.what());
        }
      }

    }

    void
    Client::_on_write_check(boost::system::error_code const& err,
                            size_t const bytes_transferred)
    {
      if (err)
      {
        _impl->connected = false;
        ELLE_WARN("trophonius has been disconnected, re-try in 10 seconds");
      }
      else if (bytes_transferred == 4)
        _impl->connected = true;
      _impl->connection_checker.expires_from_now(
        boost::posix_time::seconds(10)
      );
      _impl->connection_checker.async_wait(
          std::bind(&Client::_check_connection, this, std::placeholders::_1)
      );
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

      ELLE_DEBUG("connecting trophonius client to %s:%s",
                 _impl->server, _impl->port);
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
        if (err)
        {
          _impl->last_error = err;
          //_impl->connected = false;
        }
        ELLE_WARN("Something went wrong while reading from socket: %s", err);
        return;
      }

      ELLE_TRACE("Read %s bytes from the socket (%s available)",
                 bytes_transferred,
                 _impl->response.in_avail());

      try
        {
          ELLE_DEBUG("Received stream from trophonius.");

          // Bind stream to response.
          std::istream is(&(_impl->response));

          // Transfer socket stream to stringstream that ensure there are no
          // encoding troubles (and make the stream human readable).
          std::unique_ptr<char[]> data{new char[bytes_transferred]};
          if (!data)
            throw std::bad_alloc{};
          is.read(data.get(), bytes_transferred);
          std::string msg{data.get(), bytes_transferred};
          ELLE_DEBUG("Got message: %s", msg);

          plasma::trophonius::NotificationType notification_type =
            plasma::trophonius::NotificationType::none; // Invalid notification type.

          {
            std::stringstream ss{msg};

            Notification notification;
            elle::serialize::InputJSONArchive ar(ss, notification);
            notification_type = notification.notification_type;
          }

          std::unique_ptr<Notification> notification;
          {
            std::stringstream ss{msg};
            elle::serialize::InputJSONArchive ar{ss};
            switch (notification_type)
              {
              case NotificationType::user_status:
                notification = std::move(ar.Construct<UserStatusNotification>());
                break;
              case NotificationType::transaction:
                notification = std::move(ar.Construct<TransactionNotification>());
                break;
              case NotificationType::message:
                notification = std::move(ar.Construct<MessageNotification>());
                break;
              case NotificationType::network_update:
                notification = std::move(ar.Construct<NetworkUpdateNotification>());
                break;
              case NotificationType::connection_enabled:
                notification = std::move(ar.Construct<Notification>());
                break;
              default:
                ELLE_WARN("unknown notification %s", notification_type);
              };
          }

          if (notification)
            this->_notifications.push(std::move(notification));

          this->_read_socket();
        }
      catch (std::runtime_error const& err)
        {
          throw elle::HTTPException{
            elle::ResponseCode::bad_content, err.what()
          };
        }
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
          {"_id", _id},
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
        throw elle::HTTPException(elle::ResponseCode::error, "Writting socket error");

      this->_read_socket();
      return true;
    }

    std::unique_ptr<Notification>
    Client::poll()
    {
      // Poll while something has to be done
      if (size_t count = _impl->io_service.poll())
      {
        ELLE_DEBUG("polling io service has triggered %s events", count);
      }

      std::unique_ptr<Notification> ret;

      if (!_notifications.empty())
        {
          ELLE_TRACE("Pop notification dictionnary to be handle.");

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
#include <oracle/disciples/meta/notification_type.hh.inc>
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
#include <oracle/disciples/meta/resources/network_update.hh.inc>
#undef NETWORK_UPDATE
      }

      return out;
    }

  }
}
