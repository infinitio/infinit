#include <wrappers/boost/python.hh>
#include <surface/crust/Network.hh>

static
boost::python::object
_local_list(std::string const& path)
{
  boost::python::list networks;

  for (auto const& net: Network::list(boost::filesystem::path{path}))
    networks.append(boost::python::str(net));

  return networks;
}

static
boost::python::object
_remote_list(std::string const& host,
             uint16_t port,
             std::string const& token_path)
{
  boost::python::list networks;

  for (auto const& net: Network::list(host,
                                      port,
                                      boost::filesystem::path{token_path}))
    networks.append(boost::python::str(net));

  return networks;
}

static
std::string
_lookup(std::string const& owner_handle,
        std::string const& network_name,
        std::string const& host,
        uint16_t port,
        std::string const& token_path)
{
  return Network::lookup(owner_handle,
                         network_name,
                         host,
                         port,
                         boost::filesystem::path{token_path});
}

uint16_t
_mount(Network* net,
       std::string const& path,
       bool run)
{
  return net->mount(boost::filesystem::path(path), run);
}

void
_store(Network* net,
       std::string const& path,
       bool force)
{
  net->store(boost::filesystem::path(path), force);
}

void
_erase(Network* net,
       std::string const& path)
{
  net->erase(boost::filesystem::path(path));
}

void
_install(Network* net,
         std::string const& path)
{
  net->install(boost::filesystem::path(path));
}

void
_uninstall(Network* net,
           std::string const& path)
{
  net->uninstall(boost::filesystem::path(path));
}


Network*
_new_network_from_path(std::string const& path)
{
  return new Network(boost::filesystem::path(path));
}

Network*
_new_network(std::string const& name,
             std::string const& identity_path,
             std::string const& identity_passphrase,
             std::string const& model,
             std::string const& openness,
             std::string const& policy)
{
  // XXX: For the moment, I choosed to remove the Authority, taking meta by
  // default. This could be improved.
  return new Network(name,
                     boost::filesystem::path(identity_path),
                     identity_passphrase,
                     hole::Model(model),
                     hole::openness_from_name(openness),
                     horizon::policy_from_name(policy));
}

BOOST_PYTHON_MODULE(_crust)
{
  namespace py = boost::python;
  typedef py::return_value_policy<py::return_by_value> by_value;
  typedef py::return_value_policy<py::copy_const_reference> by_copy;

  py::enum_<plasma::meta::Client::DescriptorList>("descriptor_list")
    .value("ALL", plasma::meta::Client::DescriptorList::all)
    .value("MINE", plasma::meta::Client::DescriptorList::mine)
    .value("OTHER", plasma::meta::Client::DescriptorList::other)
  ;

  py::class_<ID>("ID",
                 py::init<size_t>())
    .def(py::init<std::string const&>())
  ;

  py::class_<Network, boost::noncopyable>(
    "_Network",
    // Constructor with id, meta host, port and token.
    py::init<ID const&, std::string const&, int16_t, std::string const&>())
    // Constructor with id, meta host and port.
    .def(py::init<ID const&, std::string const&, int16_t>())
    // Constructor with id and meta host.
    .def(py::init<ID const&, std::string const&>())
    // Constructor with id and meta host.
    .def(py::init<ID const&>())
    // Constructor with name, user, passphrase, model, policy, opennes.
    .def("__init__", boost::python::make_constructor(&_new_network))
    // Constructor with descritpor path.
    .def("__init__", boost::python::make_constructor(&_new_network_from_path))
    // Store the descriptor to the given path.
    .def("_store", py::make_function(&_store))
    // Delete the given descriptor. (rm descriptor).
    .def("_erase", py::make_function(&_erase))
    // Create the directory containing the network shelter.
    .def("_install", py::make_function(&_install))
    // Delete the directory containing the network shelter (rm -r networkpath).
    .def("_uninstall", py::make_function(&_uninstall))
    // Prepare the mount point and launch infinit if asked.
    .def("_mount", py::make_function(&_mount))
    // Unmount infinit.
    .def("_unmount", &Network::unmount)
    // Publish the descriptor to the remote.
    .def("_publish", &Network::publish)
    // Unpublish the descriptor from the remote.
    .def("_unpublish", &Network::unpublish)

    .add_property("identifier", py::make_function(&Network::identifier, by_value()))
    .add_property("administrator_K", py::make_function(&Network::administrator_K, by_value()))
    .add_property("model", py::make_function(&Network::model, by_value()))
    .add_property("everybody_identity", py::make_function(&Network::everybody_identity, by_value()))
    .add_property("history", py::make_function(&Network::history, by_value()))
    .add_property("extent", py::make_function(&Network::extent, by_value()))

    .add_property("name", py::make_function(&Network::name, by_value()))
    .add_property("openness", py::make_function(&Network::openness, by_value()))
    .add_property("policy", py::make_function(&Network::policy, by_value()))
    .add_property("version", py::make_function(&Network::version, by_value()))
  ;
  // List local descriptor from a given path.
  py::def("_local_list", py::make_function(&_local_list));
  // List the descriptor stored on the remote.
  py::def("_remote_list", py::make_function(&_remote_list));
  // Lookup descriptor id from owner handle and network name.
  py::def("_lookup", py::make_function(&_lookup));


  //py::def("validate", &Network::validate);
}
