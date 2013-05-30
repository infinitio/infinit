#include "NotificationManager.hh"

#include <elle/os/path.hh>
#include <elle/printf.hh>
#include <elle/format/json.hh>
#include <elle/format/json/Dictionary.hxx>
#include <elle/Exception.hh>

#include <fstream>

#include <common/common.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.Notification");

namespace surface
{
  namespace gap
  {
    namespace json = elle::format::json;

    NotificationManager::NotificationManager(plasma::meta::Client& meta,
                                             Self const& self,
                                             Device const& device):
      _meta(meta),
      _self(self),
      _device(device)
    {
      ELLE_TRACE_METHOD("");
      this->_connect();
    }

    NotificationManager::~NotificationManager()
    {
      ELLE_TRACE_METHOD("");
    }

    // - TROPHONIUS ----------------------------------------------------
    /// Connect to trophonius
    ///

    // This method connect the state to trophonius.
    //
    // Trophonius is the notification system of Infinit Beam.
    //
    // The connection is made in two steps:
    //  - We look for the file in ${USER_DIR}/erginus.sock for the port number
    //    of the erginus server (if launched).
    //    - if it's not: we start it TODO
    //  - We use common::trophonius::{port, host} if the file is not there
    void
    NotificationManager::_connect()
    {
      ELLE_ASSERT_EQ(this->_trophonius, nullptr);

      try
      {
        uint16_t port;
        std::string port_file_path = elle::os::path::join(
            common::infinit::user_directory(this->_self.id),
            "erginus.sock");

        if (elle::os::path::exists(port_file_path))
        {
          // we can read it and get the port number;
          std::ifstream file(port_file_path);
          ELLE_DEBUG("erginus port file is at %s", port_file_path);

          if (file.good())
          {
            file >> port;
            ELLE_DEBUG("erginus port is %s", port);
            this->_trophonius.reset(
              new plasma::trophonius::Client{"localhost", port, true});
            ELLE_DEBUG("successfully connected to erginus");
          }
          return ;
        }
        ELLE_DEBUG("erginus port file not found");
      }
      catch (std::runtime_error const& err)
      {
        ELLE_DEBUG("couldn't connect to erginus: %s", err.what());
      }

      try
      {
        this->_trophonius.reset(
          new plasma::trophonius::Client{
            common::trophonius::host(),
            common::trophonius::port(),
            true,
          }
        );
      }
      catch (...)
      {
        std::string err = elle::sprint("Couldn't connect to trophonius:",
                                       elle::exception_string());
        ELLE_ERR(err);
        throw NotificationManager::Exception{gap_error, err};
      }
      this->_trophonius->connect(this->_self.id,
                                 this->_meta.token(),
                                 this->_device.id);

      ELLE_LOG("Connect to trophonius: id = %s token = %s device_id = %s",
               this->_self.id,
               this->_meta.token(),
               this->_device.id);
    }

    void
    NotificationManager::_check_trophonius()
    {
      if (this->_trophonius == nullptr)
        throw Exception{gap_error, "Trophonius is not connected"};
    }

    size_t
    NotificationManager::poll(size_t max)
    {
      std::unique_ptr<Notification> notif;
      std::string transaction_id = "";
      size_t count = 0;
      try
      {
        this->_check_trophonius();

        while (count < max && this->_trophonius != nullptr)
        {
          notif.reset(this->_trophonius->poll().release());
          transaction_id = "";

          if (!notif)
            break;
          // Try to retrieve Transaction ID if possible
          {
            if (notif->notification_type == NotificationType::transaction_status)
            {
              auto ptr = static_cast<TransactionStatusNotification*>(notif.get());
              transaction_id = ptr->transaction_id;
            }
            else if (notif->notification_type == NotificationType::transaction)
            {
              auto ptr = static_cast<TransactionNotification*>(notif.get());
              transaction_id = ptr->transaction.id;
            }
          }

          this->_handle_notification(*notif);
          transaction_id = "";
          notif.reset();
          ++count;
        }
      }
      catch (...)
      {
        std::string error = elle::sprintf("%s: %s",
                                          notif->notification_type,
                                          elle::exception_string());
        ELLE_ERR("got error while polling: %s", error);
        this->_call_error_handlers(gap_unknown, error, transaction_id);
      }
      return count;
    }

    static
    std::unique_ptr<Notification>
    _xxx_dict_to_notification(json::Dictionary const& d)
    {
      std::unique_ptr<Notification> res;
      NotificationType notification_type = (NotificationType) d["notification_type"].as_integer().value();

      std::unique_ptr<UserStatusNotification> user_status{
          new UserStatusNotification
      };

      std::unique_ptr<TransactionNotification> transaction{
          new TransactionNotification
      };
      std::unique_ptr<TransactionStatusNotification> transaction_status{
          new TransactionStatusNotification
      };
      std::unique_ptr<MessageNotification> message{
          new MessageNotification
      };

      switch (notification_type)
        {
        case NotificationType::user_status:
          user_status->user_id = d["user_id"].as_string();
          user_status->status = d["status"].as_integer();
          user_status->user_id = d["device_id"].as_string();
          user_status->status = d["device_status"].as_integer();
          res = std::move(user_status);
          break;

        case NotificationType::transaction:
#define GET_TR_FIELD_RENAME(_if_, _of_, type)                                          \
          try                                                                   \
          {                                                                     \
            ELLE_DEBUG("Get transaction field " #_if_);                            \
            transaction->transaction._of_ = d["transaction"][#_if_].as_ ## type ();   \
          }                                                                     \
          catch (...)                                                           \
          {                                                                     \
            ELLE_ERR("Couldn't get field " #_if_);                                 \
          }                                                                     \

#define GET_TR_FIELD(_if_, _type_) GET_TR_FIELD_RENAME(_if_, _if_, type)

          GET_TR_FIELD_RENAME(transaction_id, id, string);
          GET_TR_FIELD(sender_id, string);
          GET_TR_FIELD(sender_fullname, string);
          GET_TR_FIELD(sender_device_id, string);
          GET_TR_FIELD(recipient_id, string);
          GET_TR_FIELD(recipient_fullname, string);
          GET_TR_FIELD(recipient_device_id, string);
          GET_TR_FIELD(recipient_device_name, string);
          GET_TR_FIELD(network_id, string);
          GET_TR_FIELD(message, string);
          GET_TR_FIELD(first_filename, string);
          GET_TR_FIELD(files_count, integer);
          GET_TR_FIELD(total_size, integer);
          GET_TR_FIELD(timestamp, float);
          GET_TR_FIELD(is_directory, integer);
          GET_TR_FIELD(status, integer);
          // GET_TR_FIELD(already_accepted, integer);
          // GET_TR_FIELD(early_accepted, integer);
          res = std::move(transaction);
          break;

        case NotificationType::transaction_status:
          transaction_status->transaction_id = d["transaction_id"].as_string();
          transaction_status->status = d["status"].as_integer();
          res = std::move(transaction_status);
          break;

        case NotificationType::message:
          message->sender_id = d["sender_id"].as_string();
          message->message = d["message"].as_string();
          res = std::move(message);
          break;

        case NotificationType::connection_enabled:
          res.reset(new Notification);
          break;

        default:
          throw elle::Exception{
              elle::sprintf("Unknown notification type %s", notification_type)
          };
        }
      res->notification_type = notification_type;
      return res;
    }

    void
    NotificationManager::pull(size_t count,
                              size_t offset,
                              bool only_new)
    {
      ELLE_TRACE_FUNCTION(count, offset);

      auto res = this->_meta.pull_notifications(count, offset);

      ELLE_DEBUG("pulled %s new and %s old notifications",
                 res.notifs.size(),
                 res.old_notifs.size());

      if (!only_new)
        for (auto const& notification: res.old_notifs)
          this->_handle_notification(notification, false);

      for (auto const& notification: res.notifs)
        this->_handle_notification(notification, true);
    }

    void
    NotificationManager::read()
    {
      this->_meta.notification_read();
    }

    void
    NotificationManager::_handle_notification(json::Dictionary const& dict,
                                              bool new_)
    {
      this->_check_trophonius();

      try
      {
        this->_handle_notification(*_xxx_dict_to_notification(dict), new_);
      }
      catch (...)
      {
        ELLE_ERR("couldn't handle notification: %s: %s",
                 dict.repr(),
                 elle::exception_string());
      }
      ELLE_DEBUG("End of notification pull");
    }

    void
    NotificationManager::_handle_notification(Notification const& notif,
                                              bool new_)
    {
      ELLE_DEBUG_SCOPE("Handling notification");
      this->_check_trophonius();

      // Connexion established.
      if (notif.notification_type == NotificationType::connection_enabled)
        // XXX set _connection_enabled to true
        return;

      auto handler_list = _notification_handlers.find(notif.notification_type);

      if (handler_list == _notification_handlers.end())
        {
          ELLE_DEBUG("Handler missing for notification '%u'",
                     notif.notification_type);
          return;
        }

      for (auto& handler: handler_list->second)
        {
          ELLE_DEBUG("Firing notification handler (piupiu)");
          ELLE_ASSERT(handler != nullptr);
          handler(notif, new_);
          ELLE_DEBUG("Notification handler fired (piupiu done)");
        }
    }

    void
    NotificationManager::network_update_callback(NetworkUpdateNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<NetworkUpdateNotification const&>(notif));
      };

      this->_notification_handlers[NotificationType::network_update].push_back(fn);
    }

    void
    NotificationManager::transaction_callback(TransactionNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool is_new) -> void {
        return cb(static_cast<TransactionNotification const&>(notif), is_new);
      };

      this->_notification_handlers[NotificationType::transaction].push_back(fn);
    }

    void
    NotificationManager::transaction_status_callback(TransactionStatusNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool is_new) -> void {
        return cb(static_cast<TransactionStatusNotification const&>(notif), is_new);
      };

      _notification_handlers[NotificationType::transaction_status].push_back(fn);
    }

    void
    NotificationManager::message_callback(MessageNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<MessageNotification const&>(notif));
      };

      this->_notification_handlers[NotificationType::message].push_back(fn);
    }

    void
    NotificationManager::user_status_callback(UserStatusNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<UserStatusNotification const&>(notif));
      };

      this->_notification_handlers[NotificationType::user_status].push_back(fn);
    }

    void
    NotificationManager::on_error_callback(OnErrorCallback const& cb)
    {
       this->_error_handlers.push_back(cb);
    }

    void
    NotificationManager::_call_error_handlers(gap_Status status,
                                              std::string const& s,
                                              std::string const& tid)
    {
      this->_check_trophonius();

      for (auto const& c: this->_error_handlers)
        try
        {
            c(status, s, tid);
        }
        catch (...)
        {
          ELLE_ERR("error handler threw an error: %s",
                   elle::exception_string());
        }
    }

    Notifiable::Notifiable(NotificationManager& notification_manager):
      _notification_manager(notification_manager)
    {}
  }
}
