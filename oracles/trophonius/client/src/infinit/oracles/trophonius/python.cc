#include <boost/python.hpp>

#include <plasma/trophonius/Client.hh>

using namespace boost::python;

class Client: public plasma::trophonius::Client
{
public:
  Client(std::string const& host, uint16_t port, object on_connection):
    plasma::trophonius::Client(host, port,
                               [=](bool value) { on_connection(value); })
  {}

  int
  ping_period_get() const
  {
    return this->ping_period().total_seconds();
  }

  void
  ping_period_set(int period)
  {
    this->ping_period(boost::posix_time::seconds(period));
  }
};

struct NotificationConverter
{
  static
  PyObject*
  convert(std::unique_ptr<plasma::trophonius::Notification> const& notif);
};

struct NotificationTypeConverter
{
  static
  PyObject*
  convert(plasma::trophonius::NotificationType type)
  {
    return incref(object(elle::sprintf("%s", type)).ptr());
  }
};

using plasma::trophonius::Notification;
using plasma::trophonius::UserStatusNotification;
using plasma::trophonius::TransactionNotification;
using plasma::trophonius::NetworkUpdateNotification;
using plasma::trophonius::MessageNotification;

void export_trophonius();
void export_trophonius()
{
  to_python_converter<plasma::trophonius::NotificationType,
    NotificationTypeConverter> notificationtype_converter;

  to_python_converter<std::unique_ptr<plasma::trophonius::Notification>,
    NotificationConverter> notification_converter;

  class_<Client, boost::noncopyable>(
    "Trophonius", init<std::string const&, uint16_t, object>())
    .def("connect", &Client::connect)
    .def("poll", &Client::poll)
    .add_property("retries", &Client::reconnected)
    .add_property("ping_period",
                  &Client::ping_period_get, &Client::ping_period_set)
    ;

  class_<plasma::trophonius::Notification>(
    "Notification", no_init)
    .def_readonly("type", &plasma::trophonius::Notification::notification_type)
    ;

  class_<UserStatusNotification,
         bases<plasma::trophonius::Notification>>(
    "UserStatusNotification", no_init)
    .def_readonly("user_id", &UserStatusNotification::user_id)
    .def_readonly("status", &UserStatusNotification::status)
    .def_readonly("device_id", &UserStatusNotification::device_id)
    .def_readonly("device_status", &UserStatusNotification::device_status)
    ;

  class_<TransactionNotification,
         bases<plasma::trophonius::Notification>>(
    "TransactionNotification", no_init)
    ;

  class_<NetworkUpdateNotification,
         bases<plasma::trophonius::Notification>>(
    "NetworkUpdateNotification", no_init)
    .def_readonly("network_id", &NetworkUpdateNotification::network_id)
    .def_readonly("what", &NetworkUpdateNotification::what)
    ;

  class_<MessageNotification,
         bases<plasma::trophonius::Notification>>(
    "MessageNotification", no_init)
    .def_readonly("sender_id", &MessageNotification::sender_id)
    .def_readonly("message", &MessageNotification::message)
    ;
}

PyObject*
NotificationConverter::convert(
  std::unique_ptr<plasma::trophonius::Notification> const& notif_)
{
  auto& notif = const_cast<std::unique_ptr<Notification>&>(notif_);

  if (!notif)
    // Return None.
    return incref(object().ptr());

#define CASE(Type)                                              \
  if (Type* o = dynamic_cast<Type*>(notif.get()))               \
  {                                                             \
    notif.release();                                            \
    return incref(boost::python::object(*o).ptr());             \
  }                                                             \
  else                                                          \

  CASE(MessageNotification)
  CASE(NetworkUpdateNotification)
  CASE(TransactionNotification)
  CASE(UserStatusNotification)
  CASE(Notification)
  return incref(boost::python::object(*notif.release()).ptr());
}
