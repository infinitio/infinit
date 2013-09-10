#ifndef PLASMA_TROPHONIUS_CLIENT_HH
# define PLASMA_TROPHONIUS_CLIENT_HH

# include <plasma/plasma.hh>

# include <elle/HttpClient.hh>

# include <boost/date_time/posix_time/posix_time_types.hpp>
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

      virtual
      void
      print(std::ostream& stream) const override;
    };

    struct NetworkUpdateNotification:
      public Notification
    {
      std::string network_id;
      NetworkUpdate what;
      ELLE_SERIALIZE_CONSTRUCT(NetworkUpdateNotification,
                               Notification)
      {}
    };

    struct PeerConnectionUpdateNotification:
      public Notification
    {
      std::string transaction_id;
      bool status;
      std::vector<std::string> devices;

      ELLE_SERIALIZE_CONSTRUCT(PeerConnectionUpdateNotification,
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

    class Client:
      public elle::Printable
    {
    public:
      typedef std::function<void (bool)> ConnectCallback;
    private:
      struct Impl;
      std::unique_ptr<Impl> _impl;

    public:
      Client(std::string const& server,
             uint16_t port,
             ConnectCallback connect_callback);

      ~Client();

    public:
      bool
      connect(std::string const& _id,
              std::string const& token,
              std::string const& device_id);

      void
      disconnect();

      //GenericNotification
      std::unique_ptr<Notification>
      poll();

      int
      reconnected() const;
      ELLE_ATTRIBUTE_rw(boost::posix_time::time_duration, ping_period);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const override;
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
