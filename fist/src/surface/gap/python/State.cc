#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>
#include <boost/uuid/string_generator.hpp>

#include <elle/assert.hh>
#include <elle/os/path.hh>
#include <elle/python/containers.hh>
#include <elle/system/home_directory.hh>

#include <common/common.hh>

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
    std::vector<elle::UUID> connected_devices;
    for (int i = 0; i < PyList_Size(device_list); i++)
    {
      connected_devices.push_back(
        elle::UUID(PyUnicode_AS_DATA(PyList_GetItem(device_list, i))));
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
    for (auto const& device: user.connected_devices)
    {
      PyList_Append(
        device_list,
        PyUnicode_FromString(boost::lexical_cast<std::string>(device).data()));
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
    case Transaction::Status::ghost_uploaded:
      return "ghost-uploaded";
    case Transaction::Status::cloud_buffered:
      return "cloud-buffered";
    case Transaction::Status::deleted:
      return "deleted";
  }
  elle::unreachable();
}

struct transaction_to_python_dict
{
  static
  PyObject*
  convert(surface::gap::Transaction const& transaction)
  {
    infinit::oracles::Transaction const* data = transaction.data().get();
    auto dict = PyDict_New();
    PyDict_SetItemString(
      dict, "id",
      PyUnicode_FromString(data->id.data()));
    PyDict_SetItemString(
      dict, "sender_id",
      PyUnicode_FromString(data->sender_id.data()));
    PyDict_SetItemString(
      dict, "sender_device_id",
      PyUnicode_FromString(
        boost::lexical_cast<std::string>(data->sender_device_id).data()));
    PyDict_SetItemString(
      dict, "is_ghost",
      PyBool_FromLong(data->is_ghost));
    PyDict_SetItemString(
      dict, "status",
      PyUnicode_FromString(
        transaction_status_string(data->status).data()));
    PyDict_SetItemString(
      dict, "ctime",
      PyFloat_FromDouble(data->ctime));
    PyDict_SetItemString(
      dict, "mtime",
      PyFloat_FromDouble(data->mtime));
    infinit::oracles::PeerTransaction const* peer_data
      = dynamic_cast<infinit::oracles::PeerTransaction const*>(data);
    if (peer_data)
    {
      auto file_list = PyList_New(0);
      for (std::string const& file: peer_data->files)
      {
        PyList_Append(file_list, PyUnicode_FromString(file.data()));
      }
      PyDict_SetItemString(dict, "files", file_list);
      PyDict_SetItemString(dict, "is_directory",
        PyBool_FromLong(peer_data->is_directory));
      PyDict_SetItemString(dict, "files_count",
        PyLong_FromLongLong(peer_data->files_count));
      PyDict_SetItemString(dict, "message",
        PyUnicode_FromString(peer_data->message.data()));
      PyDict_SetItemString(dict, "recipient_id",
        PyUnicode_FromString(peer_data->recipient_id.data()));
      PyDict_SetItemString(dict, "recipient_fullname",
        PyUnicode_FromString(peer_data->recipient_fullname.data()));
      PyDict_SetItemString(dict, "recipient_device_id",
        PyUnicode_FromString(boost::lexical_cast<std::string>(peer_data->recipient_device_id).data()));
      PyDict_SetItemString(dict, "recipient_device_name",
        PyUnicode_FromString(peer_data->recipient_device_name.data()));
      PyDict_SetItemString(dict, "sender_fullname",
        PyUnicode_FromString(peer_data->sender_fullname.data()));
      PyDict_SetItemString(dict, "total_size",
        PyLong_FromLongLong(peer_data->total_size));
    }
    infinit::oracles::LinkTransaction const* link_data
      = dynamic_cast<infinit::oracles::LinkTransaction const*>(data);
    if (link_data)
    {
      PyDict_SetItemString(dict, "click_count",
        PyLong_FromLongLong(link_data->click_count));
      auto file_list = PyList_New(0);
      for (auto const& file: link_data->file_list)
      {
        auto entry =  PyList_New(0);
        PyList_Append(entry, PyUnicode_FromString(file.first.data()));
        PyList_Append(entry, PyLong_FromLongLong(file.second));
        PyList_Append(file_list, entry);
      }
      PyDict_SetItemString(dict, "files", file_list);
      PyDict_SetItemString(dict, "mtime", PyFloat_FromDouble(link_data->mtime));
      PyDict_SetItemString(dict, "name",
        PyUnicode_FromString(link_data->name.data()));
      PyDict_SetItemString(dict, "share_link",
        PyUnicode_FromString(link_data->share_link.data()));
    }
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
    case gap_transaction_new:
      return "transaction_new";
    case gap_transaction_on_other_device:
      return "transaction_on_other_device";
    case gap_transaction_waiting_accept:
      return "transaction_waiting_accept";
    case gap_transaction_waiting_data:
      return "transaction_waiting_data";
    case gap_transaction_connecting:
      return "transaction_connecting";
    case gap_transaction_transferring:
      return "transaction_transferring";
    case gap_transaction_cloud_buffered:
      return "transaction_cloud_buffered";
    case gap_transaction_finished:
      return "transaction_finished";
    case gap_transaction_failed:
      return "transaction_failed";
    case gap_transaction_canceled:
      return "transaction_canceled";
    case gap_transaction_rejected:
      return "transaction_rejected";
    case gap_transaction_deleted:
      return "transaction_deleted";
  }
  elle::unreachable();
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

struct cxx_map_to_python_dict
{
  static
  PyObject*
  convert(std::unordered_map<std::string, std::string> const& map)
  {
    auto dict = PyDict_New();
    for (auto const& item: map)
       PyDict_SetItemString(dict, item.first.c_str(), PyUnicode_FromString(
         item.second.c_str()));
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

  boost::python::to_python_converter<std::unordered_map<std::string, std::string>,
                                     cxx_map_to_python_dict>();
}

class PythonState:
  public common::infinit::Configuration,
  public surface::gap::State
{
public:
  typedef surface::gap::State Super;
  PythonState(std::string const& meta_protocol,
              std::string const& meta_host,
              uint16_t meta_port,
              std::string const& download_dir,
              std::string const& did)
  : common::infinit::Configuration(false)
  , Super(meta_protocol,
          meta_host,
          meta_port,
          did.empty()?device_id() : boost::uuids::string_generator()(did),
          trophonius_fingerprint(),
          download_dir)
  {}

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
  void
  attach_connection_callback(boost::python::object cb) const
  {
    Super::attach_callback<surface::gap::State::ConnectionStatus>(
      [cb] (surface::gap::State::ConnectionStatus const& notif)
      {
        auto dict = PyDict_New();
        PyDict_SetItemString(dict, "last_error", PyUnicode_FromString(notif.last_error.c_str()));
        PyDict_SetItemString(dict, "status", notif.status? Py_True : Py_False);
        PyDict_SetItemString(dict, "still_trying", notif.still_trying? Py_True : Py_False);
        boost::python::handle<> handle(dict);
        boost::python::object odict(handle);
        cb(odict);
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
    return send_files(peer, files, message);
  }

  uint32_t
  wrap_create_link(std::vector<std::string> files,
    const std::string& message)
  {
    return create_link(files, message);
  }

  std::string
  wrap_transaction_status(unsigned int id)
  {
    return gap_transaction_status_string(transactions().at(id)->status());
  }

  PyObject*
  transaction(unsigned int id)
  {
    return transaction_to_python_dict::convert(*transactions().at(id));
  }

  PyObject*
  find_transaction(std::string const& uid)
  {
    auto t = std::find_if(transactions().begin(), transactions().end(),
      [&](Transactions::value_type const& v)
      {
        return v.second->data()->id == uid;
      });
    if (t == transactions().end())
      return PyLong_FromLongLong(-1);
    return PyLong_FromLongLong(t->first);
  }

  void
  configuration_set_max_mirror_size(uint64_t sz)
  {
    Super::_configuration.max_mirror_size = sz;
  }

  std::unordered_map<std::string, std::string>
  features()
  {
    return _configuration.features;
  }

#define TOP(name, ret)                             \
  ret transaction_ ## name(unsigned int id)        \
  {                                                \
    return transactions().at(id)->name();          \
  }

  TOP(accept, void)
  TOP(reject, void)
  TOP(cancel, void)
  TOP(join, void)
  TOP(progress, float)
  TOP(pause, bool)
  TOP(interrupt, void)
  TOP(reset, void)
  TOP(final, bool)
  #undef TOP
};




BOOST_PYTHON_MODULE(state)
{
  namespace py = boost::python;
  typedef
    py::return_value_policy<boost::python::copy_const_reference> by_const_ref;
   typedef
    py::return_value_policy<boost::python::return_by_value> by_value;

  // Binding of some standard types we use.
  elle::python::container_to_python<std::vector<unsigned int>>();
  elle::python::container<std::vector<std::string>>();

  bind_conversions();

  using surface::gap::State;
  typedef infinit::oracles::meta::User User;


  boost::python::class_<PythonState, boost::noncopyable>
    ("State",
     boost::python::init<std::string const&,
                         std::string const&,
                         uint16_t,
                         std::string const&,
                         std::string const&>())
    .def("logged_in", &State::logged_in_to_meta)
    .def("login", (void (State::*)(std::string const&, std::string const&)) &State::login)
    .def("logout", &State::logout)
    .def("poll", &State::poll)
    .def("users", &State::users, by_const_ref())
    .def("transaction", &PythonState::transaction)
    .def("transaction_accept", &PythonState::transaction_accept)
    .def("transaction_reject", &PythonState::transaction_reject)
    .def("transaction_join", &PythonState::transaction_join)
    .def("transaction_cancel", &PythonState::transaction_cancel)
    .def("transaction_progress", &PythonState::transaction_progress)
    .def("transaction_pause", &PythonState::transaction_pause)
    .def("transaction_interrupt", &PythonState::transaction_interrupt)
    .def("transaction_reset", &PythonState::transaction_reset)
    .def("transaction_final", &PythonState::transaction_final)
    .def("transaction_status", &PythonState::wrap_transaction_status)
    .def("swaggers", &PythonState::wrap_swaggers)
    .def("swagger_from_name", (User (State::*)(const std::string&)) &State::swagger)
    .def("swagger_from_id", (User (State::*) (uint32_t)) &State::swagger)
    .def("device_status", &State::device_status)
    .def("transactions", (const State::Transactions& (State::*)() const)&State::transactions, by_value())
    .def("send_files", &PythonState::wrap_send_files)
    .def("create_link", &PythonState::wrap_create_link)
    .def("find_transaction", &PythonState::find_transaction)
    .def("transactions", static_cast<
      State::Transactions const& (State::*)() const>(&State::transactions),
         by_const_ref())
    .def("attach_transaction_callback",
         &PythonState::attach_callback<surface::gap::Transaction::Notification>)
    .def("attach_connection_callback", &PythonState::attach_connection_callback)
    .def("configuration_set_max_mirror_size",
         &PythonState::configuration_set_max_mirror_size)
    .def("features", &PythonState::features)
    .def("register", &State::register_)
    .def("reconnection_cooldown",
      static_cast<void(State::*)(boost::posix_time::time_duration const&)>
      (&State::reconnection_cooldown))
    .def("change_password", &State::change_password)
    ;
}
