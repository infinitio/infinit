#ifndef PLASMA_TROPHONIUS_CLIENT_HH
# define PLASMA_TROPHONIUS_CLIENT_HH

# include <plasma/plasma.hh>

# include <elle/HttpClient.hh>

# include <boost/system/error_code.hpp>

# include <functional>
# include <queue>

namespace plasma
{
  namespace trophonius
  {
    enum class NotificationType: int
    {
# define NOTIFICATION_TYPE(name, value)         \
      name = value,
# include <oracle/disciples/meta/src/meta/notification_type.hh.inc>
# undef NOTIFICATION_TYPE
    };

    enum class NetworkUpdate: int
    {
# define NETWORK_UPDATE(name, value)         \
      name = value,
# include <oracle/disciples/meta/src/meta/resources/network_update.hh.inc>
# undef NETWORK_UPDATE
    };

    /// Base class for all notifications.
    struct Notification: public elle::Printable
    {
      NotificationType notification_type;

      ELLE_SERIALIZE_CONSTRUCT(Notification)
      {}

      Notification(NotificationType const type):
        notification_type{type}
      {}

      virtual ~Notification();

      virtual
      void
      print(std::ostream& stream) const override;
    };

    namespace json = elle::format::json;

    struct NewSwaggerNotification:
      public Notification
    {
      std::string user_id;

      ELLE_SERIALIZE_CONSTRUCT(NewSwaggerNotification,
                               Notification)
      {}
    };

    struct UserStatusNotification:
      public Notification
    {
      std::string user_id;
      bool status;
      std::string device_id;
      bool device_status;

      ELLE_SERIALIZE_CONSTRUCT(UserStatusNotification,
                               Notification)
      {}

      UserStatusNotification():
        Notification{NotificationType::user_status}
      {}
    };

    struct TransactionNotification:
      public Notification,
      public Transaction
    {
      ELLE_SERIALIZE_CONSTRUCT(TransactionNotification,
                               Notification,
                               Transaction)
      {}
    };

    struct NetworkUpdateNotification:
      public Notification
    {
      std::string network_id;
      /* NetworkUpdate */ int what;
      ELLE_SERIALIZE_CONSTRUCT(NetworkUpdateNotification,
                               Notification)
      {}
    };

    struct MessageNotification:
      public Notification
    {
      std::string sender_id;
      std::string message;
      ELLE_SERIALIZE_CONSTRUCT(MessageNotification,
                               Notification)
      {}
    };

    /// Build a notification with the 'good' type from a dictionnary.
    /// The notification type is determined by the "notification_type" field
    /// presents in the dictionary.
    std::unique_ptr<Notification>
    notification_from_dict(json::Dictionary const& dict);

    class Client: public elle::Printable
    {
    private:
      struct Impl;
      std::unique_ptr<Impl> _impl;

    public:
      Client(std::string const& server,
             uint16_t port,
             std::function<void()> connect_callback);

      ~Client();

    public:
      bool
      connect(std::string const& _id,
              std::string const& token,
              std::string const& device_id);

      //GenericNotification
      std::unique_ptr<Notification>
      poll();

      bool
      has_notification(void);

    private:
      std::queue<std::unique_ptr<Notification>> _notifications;

      void
      _connect();

      void
      _disconnect();

      void
      _read_socket();

      void
      _restart_ping_timer();

      void
      _restart_connection_check_timer();

      void
      _on_read_socket(boost::system::error_code const& err,
                      size_t bytes_transferred);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const override;

      void
      _check_connection(boost::system::error_code const& err);

      void
      _send_ping(boost::system::error_code const& err);

      void
      _on_ping_sent(boost::system::error_code const& err,
                      size_t const bytes_transferred);
    };

    std::ostream&
    operator <<(std::ostream& out,
                NotificationType t);

    std::ostream&
    operator <<(std::ostream& out,
                NetworkUpdate n);
  }
}

#endif
