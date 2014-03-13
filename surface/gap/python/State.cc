#include <boost/python.hpp>

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
    user->id = PyUnicode_AsUTF8(PyDict_GetItemString(pydict, "id"));
    user->fullname = PyUnicode_AsUTF8(PyDict_GetItemString(pydict, "fullname"));
    user->handle = PyUnicode_AsUTF8(PyDict_GetItemString(pydict, "handle"));
    user->public_key = PyUnicode_AsUTF8(PyDict_GetItemString(pydict,
                                        "public_key"));
    auto device_list = PyDict_GetItemString(pydict, "connected_devices");
    std::vector<std::string> connected_devices;
    for (int i = 0; i < PyList_Size(device_list); i++)
    {
      connected_devices.push_back(PyUnicode_AsUTF8(
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
  convert(infinit::oracles::meta::User user)
  {
    auto dict = PyDict_New();
    PyDict_SetItemString(dict, "id", PyUnicode_FromString(user.id.data()));
    PyDict_SetItemString(dict, "fullname",
                         PyUnicode_FromString(user.fullname.data()));
    PyDict_SetItemString(dict, "handle",
                         PyUnicode_FromString(user.handle.data()));
    PyDict_SetItemString(dict, "public_key",
                         PyUnicode_FromString(user.public_key.data()));
    auto device_list = PyList_New(user.connected_devices.size());
    for (auto device: user.connected_devices)
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
  convert(std::unordered_map<unsigned int, infinit::oracles::meta::User> user_map)
  {
    auto dict = PyDict_New();
    for (auto obj: user_map)
    {

      PyDict_SetItem(dict, PyLong_FromLong(obj.first),
                     user_to_python_dict::convert(obj.second));
    }
    return dict;
  }
};

// struct transaction_to_python_dict
// {
//   static
//   PyObject*
//   convert(infinit::oracles::Transaction transaction)
//   {
//     auto dict = PyDict_New();
//     PyDict_SetItemString(dict, "id",
//                          PyUnicode_FromString(transaction.id.data()));
//   }
// };

static
void
bind_user()
{
  using infinit::oracles::meta::User;

  user_from_python_dict();

  boost::python::to_python_converter<const User,
                                     user_to_python_dict>();

  boost::python::to_python_converter<
    const std::unordered_map<unsigned int, User>, user_map_to_python_dict>();
}

BOOST_PYTHON_MODULE(state)
{
  bind_user();
  using surface::gap::State;
  boost::python::class_<State, boost::noncopyable>
    ("State",
     boost::python::init<std::string const&,
                         std::string const&,
                         uint16_t,
                         std::string const&,
                         uint16_t>())
    .def("logged_in", &State::logged_in)
    .def("login", &State::login)
    .def("logout", &State::logout)
    .def("hash_password", &State::hash_password)
    .def("users", &State::users,
      boost::python::return_value_policy<boost::python::copy_const_reference>())
    ;
}
