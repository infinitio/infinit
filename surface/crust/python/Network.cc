#include <wrappers/boost/python.hh>
#include <surface/crust/Network.hh>

//static _list(Args&&... args)
template <typename... Args>
boost::python::object
_list(Args const&... args)
{
  boost::python::list networks_;

  // XXX: No move, no forward.
  for (auto const& net: Network::list(args...))
    networks_.append(boost::python::str(net));

  return networks_;
}

BOOST_PYTHON_MODULE(_crust)
{
  namespace py = boost::python;
  typedef py::return_value_policy<py::return_by_value> by_value;
  typedef py::return_value_policy<py::copy_const_reference> by_copy;

  py::enum_<plasma::meta::Client::DescriptorList>("descriptor_list")
    .value("all", plasma::meta::Client::DescriptorList::all)
    .value("mine", plasma::meta::Client::DescriptorList::mine)
    .value("other", plasma::meta::Client::DescriptorList::other)
  ;
  py::class_<Network, boost::noncopyable>(
    "_Network",
    // Constructor with name, user, passphrase, model, policy, opennes.
    py::init<std::string const&,
             std::string const&,
             std::string const&,
             std::string const&,
             std::string const&,
             std::string const&>())
    // Constructor with descritpor path.
    .def(py::init<boost::filesystem::path const&>())
    // Constructor with id and meta host and port and token.
    .def(py::init<std::string const&,
                  std::string const&,
                  int16_t,
                  std::string const&>())
    // Constructor with id and meta host and port.
    .def(py::init<std::string const&,
                  std::string const&,
                  int16_t>())
    // Constructor with id and meta host.
    .def(py::init<std::string const&,
                  std::string const&>())
    // Constructor with id.
    .def(py::init<std::string const&>())
    // Store the descriptor to the given path.
    .def("_store", &Network::store)
    // Delete the given descriptor. (rm descriptor).
    .def("_delete", &Network::delete_)
    //
    .def("_mount", &Network::mount)
    //
    .def("_unmount", &Network::unmount)
    // Publish the descriptor to the remote.
    .def("_publish", &Network::publish)
    // Unpublish the descriptor from the remote.
    .def("_unpublish", &Network::unpublish)

    .add_property("_identifier", py::make_function(&Network::identifier, by_value()))
    .add_property("_administrator_K", py::make_function(&Network::administrator_K, by_value()))
    .add_property("_model", py::make_function(&Network::model, by_value()))
    .add_property("_everybody_identity", py::make_function(&Network::everybody_identity, by_value()))
    .add_property("_history", py::make_function(&Network::history, by_value()))
    .add_property("_extent", py::make_function(&Network::extent, by_value()))

  ;
  // List local descriptor from a given path.
  py::def("list", &_list<std::string const&>);
  // List the descriptor stored on the remote.
  py::def("list", &_list<plasma::meta::Client::DescriptorList const&,
                         std::string const&,
                         uint16_t,
                         std::string const&>);

  //py::def("validate", &Network::validate);
}
