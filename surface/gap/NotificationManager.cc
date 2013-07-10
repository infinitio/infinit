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

    NotificationManager::NotificationManager(std::string const& trophonius_host,
                                             uint16_t trophonius_port,
                                             plasma::meta::Client& meta,
                                             SelfGetter const& self,
                                             DeviceGetter const& device):
      _trophonius{nullptr},
      _meta(meta),
      _self{self},
      _device{device}
    {
      ELLE_ASSERT(this->_self != nullptr);
      ELLE_ASSERT(this->_device != nullptr);
      ELLE_DEBUG("Self = %s", this->_self().id);
      ELLE_DEBUG("Device = %s", this->_device().id);
      this->_connect(trophonius_host, trophonius_port);
    }

    NotificationManager::~NotificationManager()
    {}

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
    NotificationManager::_connect(std::string const& trophonius_host,
                                  uint16_t trophonius_port)
    {
      ELLE_LOG("%s: connect(%s,%s)", *this, trophonius_host, trophonius_port);

      ELLE_ASSERT_EQ(this->_trophonius, nullptr);

      try
      {
        uint16_t port;
        std::string port_file_path = elle::os::path::join(
            common::infinit::user_directory(this->_self().id),
            "erginus.sock");

        if (elle::os::path::exists(port_file_path))
        {
          // we can read it and get the port number;
          std::ifstream file(port_file_path);
          ELLE_DEBUG("%s: erginus port file is at %s", *this, port_file_path);

          if (file.good())
          {
            file >> port;
            ELLE_DEBUG("%s: erginus port is %s", *this, port);
            this->_trophonius.reset(
              new plasma::trophonius::Client{
                "localhost",
                port,
                std::bind(&NotificationManager::_on_trophonius_connected, this),
              });
            ELLE_DEBUG("%s: successfully connected to erginus", *this);
          }
          return ;
        }
        ELLE_DEBUG("%s: erginus port file not found", *this);
      }
      catch (std::runtime_error const& err)
      {
        ELLE_DEBUG("%s: couldn't connect to erginus: %s", *this, err.what());
      }

      try
      {
        this->_trophonius.reset(
          new plasma::trophonius::Client{
            trophonius_host,
            trophonius_port,
            std::bind(&NotificationManager::_on_trophonius_connected, this),
          });
      }
      catch (...)
      {
        std::string err = elle::sprint("%s: couldn't connect to trophonius:",
                                       *this, elle::exception_string());
        ELLE_ERR("%s", err);
        throw Exception{gap_error, err};
      }
      ELLE_LOG("%s: trying to connect to tropho: "
               "id = %s token = %s device_id = %s",
               *this,
               this->_self().id,
               this->_meta.token(),
               this->_device().id);
      this->_trophonius->connect(this->_self().id,
                                 this->_meta.token(),
                                 this->_device().id);
    }

    void
    NotificationManager::_check_trophonius()
    {
      if (this->_trophonius == nullptr)
        throw Exception{gap_error, "Trophonius is not connected"};
    }

    void
    NotificationManager::_on_trophonius_connected()
    {
      ELLE_LOG("%s: successfully reconnected to trophonius", *this);
      this->pull(-1, 0, true);
      for (auto& cb: this->_resync_callbacks)
        cb();
    }

    void
    NotificationManager::add_resync_callback(ResyncCallback const& cb)
    {
      this->_resync_callbacks.push_back(cb);
    }

    size_t
    NotificationManager::poll(size_t max)
    {
      ELLE_DUMP("%s: polling at most %s notification", *this, max);

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
          if (notif->notification_type == NotificationType::transaction)
          {
            auto ptr = static_cast<TransactionNotification*>(notif.get());
            transaction_id = ptr->id;
          }

          this->_dispatch_notification(*notif);
          transaction_id = "";
          notif.reset();
          ++count;
        }
      }
      catch (std::exception const&)
      {
        std::string error = elle::sprintf(
          "%s: %s",
          (
            notif != nullptr ?
            elle::sprint(notif->notification_type) :
            "no notification available"
          ),
          elle::exception_string());
        ELLE_ERR("%s: got error while polling: %s", *this, error);
        this->_call_error_handlers(gap_unknown, error, transaction_id);
      }
      return count;
    }

    void
    NotificationManager::pull(size_t count,
                              size_t offset,
                              bool only_new)
    {
      ELLE_TRACE_SCOPE("%s: fetch at most %s notifications "
                       "starting from %s (only new: %s)",
                       *this, count, offset, only_new);

      auto res = this->_meta.pull_notifications(count, offset);

      ELLE_DEBUG("%s: pulled %s new and %s old notifications",
                 *this,
                 res.notifs.size(),
                 res.old_notifs.size());

      if (!only_new)
        for (auto const& notification: res.old_notifs)
          this->_dispatch_notification(notification, false);

      for (auto const& notification: res.notifs)
        this->_dispatch_notification(notification, true);
    }

    void
    NotificationManager::read()
    {
      ELLE_TRACE_SCOPE("%s: read notifications", *this);

      this->_meta.notification_read();
    }

    void
    NotificationManager::_dispatch_notification(json::Dictionary const& dict,
                                                bool const is_new)
    {
      try
      {
        this->_dispatch_notification(
          *plasma::trophonius::notification_from_dict(dict), is_new);
      }
      catch (std::exception const&)
      {
        ELLE_ERR("%s: couldn't handle notification: %s: %s",
                 *this,
                 dict.repr(),
                 elle::exception_string());
      }
    }

    void
    NotificationManager::_dispatch_notification(Notification const& notif,
                                                bool const is_new)
    {
      ELLE_TRACE_SCOPE("%s: handle notification", *this);
      ELLE_DEBUG("%s: %s notification: %s",
                 *this, is_new ? "new" : "old", notif);

      this->_check_trophonius();

      // Just a ping. We are not supposed to get one here, but if it's the case,
      // nothing to do with it. If we want to be more strict, we should throw.
      if (notif.notification_type == NotificationType::ping)
        return;

      // Connexion established.
      if (notif.notification_type == NotificationType::connection_enabled)
        // XXX set _connection_enabled to true
        return;

      auto handler_list = _notification_handlers.find(notif.notification_type);

      if (handler_list == _notification_handlers.end())
      {
        ELLE_DEBUG("%s: handler missing for notification '%u'",
                   *this,
                   notif.notification_type);
        return;
      }

      for (auto& handler: handler_list->second)
      {
        try
        {
          ELLE_DEBUG("%s: firing notification handler (piupiu)", *this);
          ELLE_ASSERT(handler != nullptr);
          handler(notif, is_new);
          ELLE_DEBUG("%s: notification handler fired (piupiu done)", *this);
        }
        catch (std::exception const&)
        {
          ELLE_ERR("%s: couldn't handle notification: %s: %s",
                   *this,
                   notif,
                   elle::exception_string());
        }
        catch (...)
        {
          ELLE_ERR("%s: couldn't handle notification: %s: fatal error: %s",
                   *this,
                   notif,
                   elle::exception_string());
          throw;
        }
      }
    }

    void
    NotificationManager::network_update_callback(
      NetworkUpdateNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<NetworkUpdateNotification const&>(notif));
      };

      using Type = NotificationType;
      this->_notification_handlers[Type::network_update].push_back(fn);
    }

    void
    NotificationManager::transaction_callback(
      TransactionNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool is_new) -> void {
        return cb(static_cast<TransactionNotification const&>(notif), is_new);
      };

      using Type = NotificationType;
      this->_notification_handlers[Type::transaction].push_back(fn);
    }

    void
    NotificationManager::message_callback(MessageNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<MessageNotification const&>(notif));
      };

      using Type = NotificationType;
      this->_notification_handlers[Type::message].push_back(fn);
    }

    void
    NotificationManager::new_swagger_callback(
      NewSwaggerNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<NewSwaggerNotification const&>(notif));
      };

      using Type = NotificationType;
      this->_notification_handlers[Type::new_swagger].push_back(fn);
    }

    void
    NotificationManager::fire_callbacks(Notification const& notif,
                                        bool const is_new)
    {
      for (auto const& cb: this->_notification_handlers[notif.notification_type])
        cb(notif, is_new);
    }

    void
    NotificationManager::user_status_callback(
      UserStatusNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<UserStatusNotification const&>(notif));
      };

      using Type = NotificationType;
      this->_notification_handlers[Type::user_status].push_back(fn);
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
          ELLE_ERR("%s: error handler threw an error: %s",
                   *this,
                   elle::exception_string());
        }
    }

    /*----------.
    | Printable |
    `----------*/
    void
    NotificationManager::print(std::ostream& stream) const
    {
      stream << "NotificationManager(" << this->_meta.email() << ")";
    }

    // ---------- Notifiable ---------------------------------------------------
    Notifiable::Notifiable(NotificationManager& notification_manager):
      _notification_manager(notification_manager)
    {}

    Notifiable::~Notifiable()
    {}
  }
}
