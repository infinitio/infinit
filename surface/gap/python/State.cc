#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include <elle/assert.hh>

#include <surface/gap/State.hh>

#include <infinit/oracles/meta/Client.hh>
#include <infinit/oracles/Transaction.hh>

extern "C"
{
  PyObject* PyInit_state();
}

struct user_from_python_dict
{
  user_from_python_dict()
  {
    boost::python::converter::registry::push_back(
      &convertible,
      &construct,
      boost::python::type_id<infinit::oracles::meta::User>());
  }

  static
  void*
  convertible(PyObject* obj_ptr)
  {
    if (!PyDict_Check(obj_ptr))
      return 0;
    return obj_ptr;
  }

  static
  void
  construct(PyObject* pydict,
            boost::python::converter::rvalue_from_python_stage1_data* data)
  {
    using infinit::oracles::meta::User;

    void* storage = (
      (boost::python::converter::rvalue_from_python_storage<User>*)
      data)->storage.bytes;
    new (storage) User;
    auto user = reinterpret_cast<User*>(storage);
    user->id =
      PyUnicode_AS_DATA(PyDict_GetItemString(pydict, "id"));
    user->fullname =
      PyUnicode_AS_DATA(PyDict_GetItemString(pydict, "fullname"));
    user->handle =
      PyUnicode_AS_DATA(PyDict_GetItemString(pydict, "handle"));
    user->public_key =
      PyUnicode_AS_DATA(PyDict_GetItemString(pydict, "public_key"));
    auto device_list = PyDict_GetItemString(pydict, "connected_devices");
    std::vector<std::string> connected_devices;
    for (int i = 0; i < PyList_Size(device_list); i++)
    {
      connected_devices.push_back(PyUnicode_AS_DATA(
                                  PyList_GetItem(device_list, i)));
    }
    user->connected_devices = connected_devices;
    data->convertible = storage;
  }
};

struct user_to_python_dict
{
  static
  PyObject*
  convert(infinit::oracles::meta::User const& user)
  {
    auto dict = PyDict_New();
    PyDict_SetItemString(dict, "id", PyUnicode_FromString(user.id.data()));
    PyDict_SetItemString(dict, "fullname",
                         PyUnicode_FromString(user.fullname.data()));
    PyDict_SetItemString(dict, "handle",
                         PyUnicode_FromString(user.handle.data()));
    PyDict_SetItemString(dict, "public_key",
                         PyUnicode_FromString(user.public_key.data()));
    auto device_list = PyList_New(0);
    for (std::string const& device: user.connected_devices)
    {
      PyList_Append(device_list, PyUnicode_FromString(device.data()));
    }
    PyDict_SetItemString(dict, "connected_devices", device_list);
    return dict;
  }
};

struct user_map_to_python_dict
{
  static
  PyObject*
  convert(std::unordered_map<unsigned int, infinit::oracles::meta::User> const&
          user_map)
  {
    auto dict = PyDict_New();
    for (auto const& obj: user_map)
    {
      PyDict_SetItem(dict, PyLong_FromLong(obj.first),
                     user_to_python_dict::convert(obj.second));
    }
    return dict;
  }
};

static
std::string
transaction_status_string(infinit::oracles::Transaction::Status status)
{
  using infinit::oracles::Transaction;
  switch (status)
  {
    case Transaction::Status::failed:
      return "failed";
    case Transaction::Status::started:
      return "started";
    case Transaction::Status::created:
      return "created";
    case Transaction::Status::rejected:
      return "rejected";
    case Transaction::Status::canceled:
      return "canceled";
    case Transaction::Status::initialized:
      return "initialized";
    case Transaction::Status::finished:
      return "finished";
    case Transaction::Status::accepted:
      return "accepted";
    case Transaction::Status::none:
      return "none";

    default:
      elle::unreachable();
  }
}

struct transaction_to_python_dict
{
  static
  PyObject*
  convert(surface::gap::Transaction const& transaction)
  {
    auto dict = PyDict_New();
    PyDict_SetItemString(
      dict, "id",
      PyUnicode_FromString(transaction.data()->id.data()));
    PyDict_SetItemString(
      dict, "sender_id",
      PyUnicode_FromString(transaction.data()->sender_id.data()));
    PyDict_SetItemString(
      dict, "sender_fullname",
      PyUnicode_FromString(transaction.data()->sender_fullname.data()));
    PyDict_SetItemString(
      dict, "sender_device_id",
      PyUnicode_FromString(transaction.data()->sender_device_id.data()));
    PyDict_SetItemString(
      dict, "recipient_id",
      PyUnicode_FromString(transaction.data()->recipient_id.data()));
    PyDict_SetItemString(
      dict, "recipient_fullname",
      PyUnicode_FromString(transaction.data()->recipient_fullname.data()));
    PyDict_SetItemString(
      dict, "recipient_device_id",
      PyUnicode_FromString(transaction.data()->recipient_device_id.data()));
    PyDict_SetItemString(
      dict, "recipient_device_name",
      PyUnicode_FromString(transaction.data()->recipient_device_name.data()));
    PyDict_SetItemString(
      dict, "message",
      PyUnicode_FromString(transaction.data()->message.data()));
    auto file_list = PyList_New(0);
    for (std::string const& file: transaction.data()->files)
    {
      PyList_Append(file_list, PyUnicode_FromString(file.data()));
    }
    PyDict_SetItemString(dict, "files", file_list);
    PyDict_SetItemString(
      dict, "files_count",
      PyLong_FromLong(transaction.data()->files_count));
    PyDict_SetItemString(
      dict, "total_size",
      PyLong_FromLong(transaction.data()->total_size));
    PyDict_SetItemString(
      dict, "is_directory",
      PyBool_FromLong(transaction.data()->is_directory));
    PyDict_SetItemString(
      dict, "status",
      PyUnicode_FromString(
        transaction_status_string(transaction.data()->status).data()));
    PyDict_SetItemString(
      dict, "ctime",
      PyFloat_FromDouble(transaction.data()->ctime));
    PyDict_SetItemString(
      dict, "mtime",
      PyFloat_FromDouble(transaction.data()->mtime));
    return dict;
  }
};

struct transaction_map_to_python_dict
{
  static
  PyObject*
  convert(std::unordered_map<unsigned int, surface::gap::Transaction> const&
    transaction_map)
  {
    auto dict = PyDict_New();
    for (auto const& obj: transaction_map)
    {
      PyDict_SetItem(dict, PyLong_FromLong(obj.first),
                     transaction_to_python_dict::convert(obj.second));
    }
    return dict;
  }
};

static
std::string
gap_transaction_status_string(gap_TransactionStatus status)
{
  switch (status)
  {
    case gap_transaction_none:
      return "none";
    case gap_transaction_pending:
      return "pending";
    case gap_transaction_copying:
      return "copying";
    case gap_transaction_waiting_for_accept:
      return "waiting_for_accept";
    case gap_transaction_accepted:
      return "accepted";
    case gap_transaction_preparing:
      return "preparing";
    case gap_transaction_running:
      return "running";
    case gap_transaction_cleaning:
      return "cleaning";
    case gap_transaction_finished:
      return "finished";
    case gap_transaction_failed:
      return "failed";
    case gap_transaction_canceled:
      return "canceled";
    case gap_transaction_rejected:
      return "rejected";

    default:
      elle::unreachable();
  }
}

struct transaction_notification_to_python_dict
{
  static
  PyObject*
  convert(surface::gap::Transaction::Notification const& notification)
  {
    auto dict = PyDict_New();
    PyDict_SetItemString(dict, "transaction_id",
                         PyLong_FromLong(notification.id));
    PyDict_SetItemString(dict, "status", PyUnicode_FromString(
      gap_transaction_status_string(notification.status).data()));

    return dict;
  }
};

static
void
bind_conversions()
{
  // Users.
  using infinit::oracles::meta::User;
  user_from_python_dict();
  boost::python::to_python_converter<const User,
                                     user_to_python_dict>();

  boost::python::to_python_converter<
    const std::unordered_map<unsigned int, User>, user_map_to_python_dict>();

  // Transactions.
  using surface::gap::Transaction;
  boost::python::to_python_converter<const Transaction,
                                   transaction_to_python_dict>();
  boost::python::to_python_converter<
    const std::unordered_map<unsigned int, Transaction>,
    transaction_map_to_python_dict>();

  // Transaction Notification.
  using surface::gap::Transaction;
  boost::python::to_python_converter<const Transaction::Notification,
                                     transaction_notification_to_python_dict>();
}

class PythonState:
  public surface::gap::State
{
public:
  typedef surface::gap::State Super;
  using Super::Super;

  template <typename T>
  void
  attach_callback(boost::python::object cb) const
  {
    Super::attach_callback<T>(
      [cb] (T const& notification)
      {
        boost::python::handle<> handle(
          transaction_notification_to_python_dict::convert(notification));
        boost::python::object dict(handle);
        cb(dict);
      });
  }
  std::vector<unsigned int>
  wrap_swaggers()
  {
    auto sw = swaggers();
    return std::vector<unsigned int>(sw.begin(), sw.end());
  }
  uint32_t
  wrap_send_files(const std::string& peer,
    std::vector<std::string> files,
    const std::string& message)
  {
    std::unordered_set<std::string> sfiles(files.begin(), files.end());
    return send_files(peer, std::move(sfiles), message);
  }
};




BOOST_PYTHON_MODULE(state)
{
  namespace py = boost::python;
  typedef
    py::return_value_policy<boost::python::copy_const_reference> by_const_ref;
   typedef
    py::return_value_policy<boost::python::return_by_value> by_value;
  bind_conversions();
  using surface::gap::State;
  typedef infinit::oracles::meta::User User;
  typedef infinit::oracles::Transaction Transaction;
  boost::python::class_<std::vector<unsigned int>>("VectorInt")
        .def(boost::python::vector_indexing_suite<std::vector<unsigned int>>() );
  boost::python::class_<PythonState, boost::noncopyable>
    ("State",
     boost::python::init<std::string const&,
                         std::string const&,
                         uint16_t,
                         std::string const&,
                         uint16_t>())
    .def("logged_in", &State::logged_in)
    .def("login", &State::login)
    .def("logout", &State::logout)
    .def("invite", &State::invite)
    .def("users", &State::users, by_const_ref())
    .def("swaggers", &PythonState::wrap_swaggers)
    .def("swagger_from_name", (User (State::*)(const std::string&)) &State::swagger)
    .def("swagger_from_id", (User (State::*) (uint32_t)) &State::swagger)
    .def("device_status", &State::device_status)
    .def("transactions", (const State::Transactions& (State::*)() const)&State::transactions, by_value())
    .def("send_files", &PythonState::wrap_send_files)
    .def("transactions", static_cast<
      State::Transactions const& (State::*)() const>(&State::transactions),
         by_const_ref())
    .def("attach_transaction_callback",
         &PythonState::attach_callback<surface::gap::Transaction::Notification>)
    ;
    // Static functions.
    boost::python::def("hash_password", &State::hash_password);
}
