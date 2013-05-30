#ifndef NOTIFICATIONMANAGER_HH
# define NOTIFICATIONMANAGER_HH

# include "Exception.hh"

# include <plasma/trophonius/Client.hh>
# include <plasma/meta/Client.hh>

# include <elle/attribute.hh>
# include <elle/format/json/fwd.hh>

# include <vector>
# include <functional>
# include <list>
# include <map>

namespace surface
{
  namespace gap
  {
    using ::plasma::trophonius::Notification;
    using ::plasma::trophonius::TransactionNotification;
    using ::plasma::trophonius::TransactionStatusNotification;
    using ::plasma::trophonius::UserStatusNotification;
    using ::plasma::trophonius::MessageNotification;
    using ::plasma::trophonius::NetworkUpdateNotification;
    using ::plasma::trophonius::NotificationType;
    using Self = ::plasma::meta::SelfResponse;
    using Device = ::plasma::meta::Device;

    namespace json = elle::format::json;

    class Notifiable;

    class NotificationManager
    {
      class Exception:
        public surface::gap::Exception
      {
      public:
        Exception(std::string const& what):
          surface::gap::Exception{gap_error, what}
        {}

        Exception(gap_Status error, std::string const& what):
          surface::gap::Exception{error, what}
        {}
      };

      ELLE_ATTRIBUTE(std::unique_ptr<plasma::trophonius::Client>,
                     trophonius);
      ELLE_ATTRIBUTE(plasma::meta::Client&, meta);
      ELLE_ATTRIBUTE(Self const&, self);
      ELLE_ATTRIBUTE(Device const&, device);

    public:
      NotificationManager(plasma::meta::Client& meta,
                          Self const& self,
                          Device const& device);

      virtual
      ~NotificationManager();

    private:
      void
      _connect();

      void
      _check_trophonius();

    private:
      typedef
        std::function<void(Notification const&, bool)>
        NotificationHandler;
      typedef
        std::map<NotificationType, std::list<NotificationHandler>>
        NotificationHandlerMap;

      NotificationHandlerMap _notification_handlers;

    public:
      typedef
        std::function<void (UserStatusNotification const&)>
        UserStatusNotificationCallback;

      typedef
        std::function<void (TransactionNotification const&, bool)>
        TransactionNotificationCallback;

      typedef
        std::function<void (TransactionStatusNotification const&, bool)>
        TransactionStatusNotificationCallback;

      typedef
        std::function<void (MessageNotification const&)>
        MessageNotificationCallback;

      typedef
        std::function<void (NetworkUpdateNotification const&)>
        NetworkUpdateNotificationCallback;

    public:
      void
      user_status_callback(UserStatusNotificationCallback const& cb);

      void
      transaction_callback(TransactionNotificationCallback const& cb);

      void
      transaction_status_callback(TransactionStatusNotificationCallback const& cb);

      void
      message_callback(MessageNotificationCallback const& cb);

      void
      network_update_callback(NetworkUpdateNotificationCallback const& cb);

      template<NotificationType type>
      void
      detach(NotificationHandler const& to_remove);

    public:
      size_t
      poll(size_t max = 10);

    public:
      void
      pull(size_t count,
           size_t offset,
           bool only_new);

      void
      read();

    private:
      void
      _handle_notification(json::Dictionary const& dict, bool new_ = true);


      void
      _handle_notification(Notification const& notif, bool _new = true);

    public:
      typedef
      std::function<void (gap_Status, std::string const&, std::string const&)>
        OnErrorCallback;

      std::vector<OnErrorCallback> _error_handlers;

      void
      on_error_callback(OnErrorCallback const& cb);

    private:
      void
      _call_error_handlers(gap_Status status,
                           std::string const& s,
                           std::string const& tid = "");
    };

    class Notifiable
    {
    protected:
      NotificationManager& _notification_manager;

      Notifiable(NotificationManager& notification_manager);
    };
  }
}

#include "NotificationManager.hxx"

#endif
