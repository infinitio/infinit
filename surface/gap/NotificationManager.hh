#ifndef NOTIFICATIONMANAGER_HH
# define NOTIFICATIONMANAGER_HH

# include "Device.hh"
# include "Exception.hh"
# include "Self.hh"
# include "usings.hh"

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
    namespace json = elle::format::json;

    class Notifiable;

    class NotificationManager: public elle::Printable
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
      typedef std::function<Self const&()> SelfGetter;
      typedef std::function<Device const&()> DeviceGetter;
      ELLE_ATTRIBUTE(SelfGetter, self);
      ELLE_ATTRIBUTE(DeviceGetter, device);

    public:
      NotificationManager(std::string const& trophonius_host,
                          uint16_t trophonius_port,
                          plasma::meta::Client& meta,
                          SelfGetter const& self,
                          DeviceGetter const& device);

      virtual
      ~NotificationManager();

    private:
      void
      _connect(std::string const& trophonius_host,
               uint16_t trophonius_port);

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
        std::function<void(NewSwaggerNotification const&)>
        NewSwaggerNotificationCallback;

      typedef
        std::function<void(UserStatusNotification const&)>
        UserStatusNotificationCallback;

      typedef
        std::function<void(TransactionNotification const&, bool)>
        TransactionNotificationCallback;

      typedef
        std::function<void(MessageNotification const&)>
        MessageNotificationCallback;

      typedef
        std::function<void(NetworkUpdateNotification const&)>
        NetworkUpdateNotificationCallback;

    public:
      void
      new_swagger_callback(NewSwaggerNotificationCallback const& cb);

      void
      user_status_callback(UserStatusNotificationCallback const& cb);

      void
      transaction_callback(TransactionNotificationCallback const& cb);

      void
      message_callback(MessageNotificationCallback const& cb);

      void
      network_update_callback(NetworkUpdateNotificationCallback const& cb);

      /// Fire notification manually.
      void
      fire_callbacks(Notification const& notif,
                     bool const is_new);

    //
    // ---------- Resync callbacks --------------------------------------------
    //
    public:
      typedef std::function<void(void)> ResyncCallback;
    private:
      std::list<ResyncCallback> _resync_callbacks;
    public:
      /// Add a callback to be notified when a resynchronization is needed.
      void
      add_resync_callback(ResyncCallback const& cb);

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

      void
      _on_trophonius_connected();
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

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };

    class Notifiable
    {
    protected:
      NotificationManager& _notification_manager;

      Notifiable(NotificationManager& notification_manager);
    };
  }
}

#endif
