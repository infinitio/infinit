#include <boost/python.hpp>

#include <plasma/trophonius/Client.hh>

using namespace boost::python;

class Client: public plasma::trophonius::Client
{
public:
  Client(std::string const& host, uint16_t port, object on_connection):
    plasma::trophonius::Client(host, port,
                               [=]() {on_connection();})
  {}
};

struct NotificationConverter
{
  static
  PyObject*
  convert(std::unique_ptr<plasma::trophonius::Notification> const& notif)
  {
    return incref(object().ptr());
  }
};

to_python_converter<std::unique_ptr<plasma::trophonius::Notification>,
                    NotificationConverter> converter;

BOOST_PYTHON_MODULE(plasma)
{
  class_<Client, boost::noncopyable>(
    "Client", init< std::string const&, uint16_t, object >())
    .def("connect", &Client::connect)
    .def("has_notification", &Client::has_notification)
    .def("poll", &Client::poll)
    ;

  class_<plasma::trophonius::Notification>(
    "Notification", no_init)
    .def_readonly("type", &plasma::trophonius::Notification::notification_type)
    ;

  using plasma::trophonius::UserStatusNotification;
  class_<UserStatusNotification, bases<plasma::trophonius::Notification>>(
    "UserStatusNotification", no_init)
    .def_readonly("user_id", &UserStatusNotification::user_id)
    .def_readonly("status", &UserStatusNotification::status)
    .def_readonly("device_id", &UserStatusNotification::device_id)
    .def_readonly("device_status", &UserStatusNotification::device_status)
    ;
}
