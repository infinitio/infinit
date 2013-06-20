#include <wrappers/boost/python.hh>

#include <surface/crust/Network.hh>
#include <surface/crust/User.hh>

/*--------.
| Network |
`--------*/

static
Network*
_Network_new(std::string const& identity_path,
             std::string const& identity_passphrase,
             std::string const& model,
             std::string const& openness,
             std::string const& policy,
             std::string const& description)
{
  // XXX: For the moment, I choosed to remove the Authority, taking meta by
  // default. This could be improved.
  return new Network(boost::filesystem::path(identity_path),
                     identity_passphrase,
                     hole::Model(model),
                     hole::openness_from_name(openness),
                     horizon::policy_from_name(policy),
                     description);
}

static
Network*
_Network_new_from_path(std::string const& path)
{
  return new Network(boost::filesystem::path(path));
}

static
Network*
_Network_new_from_home(std::string const& network_name,
                       std::string const& owner_name,
                       std::string const& infinit_home)
{
  // XXX: For the moment, I choosed to remove the Authority, taking meta by
  // default. This could be improved.
  return new Network(network_name,
                     owner_name,
                     boost::filesystem::path(infinit_home));
}

static
boost::python::object
_Network_list(std::string const& network_name,
                    std::string const& infinit_home)
{
  boost::python::list networks;

  for (auto const& net: Network::list(network_name,
                                      boost::filesystem::path{infinit_home}))
    networks.append(boost::python::str(net));

  return networks;
}

static
boost::python::object
_Network_fetch(std::string const& host,
                     uint16_t port,
                     std::string const& token_path)
{
  boost::python::list networks;

  for (auto const& net: Network::fetch(host,
                                       port,
                                       boost::filesystem::path{token_path}))
    networks.append(boost::python::str(net));

  return networks;
}

static
std::string
_Network_lookup(std::string const& owner_handle,
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

static
uint16_t
_Network_mount(Network* net,
               std::string const& path,
               bool run)
{
  return net->mount(boost::filesystem::path(path), run);
}

static
void
_Network_store(Network* net,
               std::string const& path,
               bool force)
{
  net->store(boost::filesystem::path(path), force);
}

static
void
_Network_erase(Network* net,
               std::string const& path)
{
  net->erase(boost::filesystem::path(path));
}

static
void
_Network_install(Network* net,
                 std::string const& network_name,
                 std::string const& administrator_name,
                 std::string const& infinit_home_path)
{
  net->install(network_name,
               administrator_name,
               boost::filesystem::path(infinit_home_path));
}

static
void
_Network_uninstall(Network* net,
                   std::string const& network_name,
                   std::string const& administrator_name,
                   std::string const& infinit_home_path)
{
  net->uninstall(network_name,
                 administrator_name,
                 boost::filesystem::path(infinit_home_path));
}

/*-----.
| User |
`-----*/

static
User*
_User_new_from_path(std::string const& path)
{
  return new User(boost::filesystem::path(path));
}

static
void
_User_store(User* user,
            std::string const& path,
            bool force)
{
  user->store(boost::filesystem::path(path), force);
}

static
void
_User_erase(User* user,
            std::string const& path)
{
  user->erase(boost::filesystem::path(path));
}


static
void
_User_install(User* user,
              std::string const& name,
              std::string const& infinit_home_path)
{
  user->install(name,
                boost::filesystem::path(infinit_home_path));
}

static
void
_User_uninstall(User* user,
                std::string const& name,
                std::string const& infinit_home_path)
{
  user->uninstall(name,
                  boost::filesystem::path(infinit_home_path));

}

//
// ---------- Python export ----------------------------------------------------
//
BOOST_PYTHON_MODULE(_crust)
{
  namespace py = boost::python;
  typedef py::return_value_policy<py::return_by_value> by_value;
  typedef py::return_value_policy<py::copy_const_reference> by_copy;

  /*---.
  | ID |
  `---*/

  py::class_<infinit::Identifier>("ID",
                                  py::init<size_t>())
    .def(py::init<std::string const&>())
  ;

  /*--------.
  | Network |
  `--------*/

  py::enum_<plasma::meta::Client::DescriptorList>("descriptor_list")
    .value("ALL", plasma::meta::Client::DescriptorList::all)
    .value("MINE", plasma::meta::Client::DescriptorList::mine)
    .value("OTHER", plasma::meta::Client::DescriptorList::other)
  ;

  py::class_<Network, boost::noncopyable>(
    "_Network",
    // Constructor with id, meta host, port and token.
    py::init<infinit::Identifier const&,
             std::string const&,
             int16_t,
             std::string const&>())
    // Constructor with id, meta host and port.
    .def(py::init<infinit::Identifier const&, std::string const&, int16_t>())
    // Constructor with id and meta host.
    .def(py::init<infinit::Identifier const&, std::string const&>())
    // Constructor with id and meta host.
    .def(py::init<infinit::Identifier const&>())
    // Constructor with name, user, passphrase, model, policy, opennes.
    .def("__init__", boost::python::make_constructor(&_Network_new))
    // Constructor with descritpor path.
    .def("__init__", boost::python::make_constructor(&_Network_new_from_path))
    // Constructor with name, owner and home.
    .def("__init__", boost::python::make_constructor(&_Network_new_from_home))
    // Store the descriptor to the given path.
    .def("_store", py::make_function(&_Network_store))
    // Delete the given descriptor. (rm descriptor).
    .def("_erase", py::make_function(&_Network_erase))
    // Create the directory containing the network shelter.
    .def("_install", py::make_function(&_Network_install))
    // Delete the directory containing the network shelter (rm -r networkpath).
    .def("_uninstall", py::make_function(&_Network_uninstall))
    // Prepare the mount point and launch infinit if asked.
    .def("_mount", py::make_function(&_Network_mount))
    // Unmount infinit.
    .def("_unmount", &Network::unmount)
    // Publish the descriptor to the remote.
    .def("_publish", &Network::publish)

    .add_property("identifier",
                  py::make_function(&Network::identifier, by_value()))
    .add_property("administrator_K",
                  py::make_function(&Network::administrator_K, by_value()))
    .add_property("model",
                  py::make_function(&Network::model, by_value()))
    .add_property("everybody_identity",
                  py::make_function(&Network::everybody_identity, by_value()))
    .add_property("history",
                  py::make_function(&Network::history, by_value()))
    .add_property("extent",
                  py::make_function(&Network::extent, by_value()))

    .add_property("description",
                  py::make_function(&Network::description, by_value()))
    .add_property("openness",
                  py::make_function(&Network::openness, by_value()))
    .add_property("policy",
                  py::make_function(&Network::policy, by_value()))
    .add_property("version",
                  py::make_function(&Network::version, by_value()))
  ;

  // Unpublish the descriptor from the remote.
  py::def("_Network_unpublish", py::make_function(&Network::unpublish));
  // List local descriptor from a given path.
  py::def("_Network_list", py::make_function(&_Network_list));
  // List the descriptor stored on the remote.
  py::def("_Network_fetch", py::make_function(&_Network_fetch));
  // Lookup descriptor id from owner handle and network name.
  py::def("_Network_lookup", py::make_function(&_Network_lookup));

  /*-----.
  | User |
  `-----*/
  py::class_<User, boost::noncopyable>(
    "_User",
    // Constructor with name and password.
    py::init<std::string const&, std::string const&>())
    // Constructor with descritpor path.
    .def("__init__", boost::python::make_constructor(&_User_new_from_path))
    // Store the identity to the given path.
    .def("_store", py::make_function(&_User_store))
    // Delete the given identity. (rm identity).
    .def("_erase", py::make_function(&_User_erase))
    // Create the directory containing the user data.
    .def("_install", py::make_function(&_User_install))
    // Delete the directory containing the user data including his networks.
    .def("_uninstall", py::make_function(&_User_uninstall))

    .add_property("identifier",
                  py::make_function(&User::identifier, by_value()))
    .add_property("description",
                  py::make_function(&User::description, by_value()))
  ;


}
