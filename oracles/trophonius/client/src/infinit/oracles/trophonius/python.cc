#include <boost/python.hpp>

#include <infinit/oracles/trophonius/Client.hh>

using namespace boost::python;

class Client: public infinit::oracles::trophonius::Client
{
public:
  Client(std::string const& host, uint16_t port, object on_connection):
    infinit::oracles::trophoniusClient(host, port,
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
  convert(std::unique_ptr<infinit::oracles::trophoniusNotification> const& notif);
};

struct NotificationTypeConverter
{
  static
  PyObject*
  convert(infinit::oracles::trophoniusNotificationType type)
  {
    return incref(object(elle::sprintf("%s", type)).ptr());
  }
};

using infinit::oracles::trophoniusNotification;
using infinit::oracles::trophoniusUserStatusNotification;
using infinit::oracles::trophoniusTransactionNotification;
using infinit::oracles::trophoniusNetworkUpdateNotification;
using infinit::oracles::trophoniusMessageNotification;

void export_trophonius();
void export_trophonius()
{
  to_python_converter<infinit::oracles::trophoniusNotificationType,
    NotificationTypeConverter> notificationtype_converter;

  to_python_converter<std::unique_ptr<infinit::oracles::trophoniusNotification>,
    NotificationConverter> notification_converter;

  class_<Client, boost::noncopyable>(
    "Trophonius", init<std::string const&, uint16_t, object>())
    .def("connect", &Client::connect)
    .def("poll", &Client::poll)
    .add_property("retries", &Client::reconnected)
    .add_property("ping_period",
                  &Client::ping_period_get, &Client::ping_period_set)
    ;

  class_<infinit::oracles::trophoniusNotification>(
    "Notification", no_init)
    .def_readonly("type", &infinit::oracles::trophoniusNotification::notification_type)
    ;

  class_<UserStatusNotification,
         bases<infinit::oracles::trophoniusNotification>>(
    "UserStatusNotification", no_init)
    .def_readonly("user_id", &UserStatusNotification::user_id)
    .def_readonly("status", &UserStatusNotification::status)
    .def_readonly("device_id", &UserStatusNotification::device_id)
    .def_readonly("device_status", &UserStatusNotification::device_status)
    ;

  class_<TransactionNotification,
         bases<infinit::oracles::trophoniusNotification>>(
    "TransactionNotification", no_init)
    ;

  class_<MessageNotification,
         bases<infinit::oracles::trophoniusNotification>>(
    "MessageNotification", no_init)
    .def_readonly("sender_id", &MessageNotification::sender_id)
    .def_readonly("message", &MessageNotification::message)
    ;
}

PyObject*
NotificationConverter::convert(
  std::unique_ptr<infinit::oracles::trophoniusNotification> const& notif_)
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
  CASE(TransactionNotification)
  CASE(UserStatusNotification)
  CASE(Notification)
  return incref(boost::python::object(*notif.release()).ptr());
}
