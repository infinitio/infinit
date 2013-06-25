#include <wrappers/boost/python.hh>

#include <surface/crust/MetaAuthority.hh>
#include <surface/crust/Network.hh>
#include <surface/crust/User.hh>

/*---.
| ID |
`---*/

static
boost::python::str
_ID_print(infinit::Identifier* id)
{
  boost::python::str str(id->value());
  return str;
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
User*
_User_new_from_home(std::string const& user_name,
                    std::string const& infinit_home_path,
                    bool)
{
  return new User(user_name, boost::filesystem::path{infinit_home_path});
}

static
User*
_User_new(std::string const& password,
          std::string const& description)
{
  return new User(password, description);
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
_User_erase(std::string const& path)
{
  User::erase(boost::filesystem::path(path));
}

static
void
_User_install(User* user,
              std::string const& name,
              std::string const& infinit_home_path)
{
  user->install(name, boost::filesystem::path(infinit_home_path));
}

static
void
_User_uninstall(std::string const& name,
                std::string const& infinit_home_path)
{
  User::uninstall(name,
                  boost::filesystem::path(infinit_home_path));

}

static
std::string
_User_store_token(std::string const& token,
                  std::string const& user_name,
                  std::string const& infinit_home_path)
{
  return User::store_token(token,
                           user_name,
                           boost::filesystem::path{infinit_home_path});
}


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
_Network_erase_s(std::string const& path)
{
  Network::erase(boost::filesystem::path(path));
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
_Network_uninstall_s(std::string const& network_name,
                     std::string const& administrator_name,
                     std::string const& infinit_home_path)
{
  Network::uninstall(network_name,
                     administrator_name,
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
    // Constructor with string format of the identifier.
    .def(py::init<std::string const&>())
    // Pretty printer.
    .def("__str__", py::make_function(&_ID_print))

    .add_property("value", py::make_function(&infinit::Identifier::value, by_value()))
  ;



  /*-----.
  | User |
  `-----*/
  py::class_<User, boost::noncopyable>("_User", py::no_init)
    // Constructor with password.
    .def("__init__", py::make_constructor(&_User_new))
    // Constructor with user_name and home.
    .def("__init__", py::make_constructor(&_User_new_from_home))
    // Constructor with descritpor path.
    .def("__init__", py::make_constructor(&_User_new_from_path))
    // Store the identity to the given path.
    .def("_store", py::make_function(&_User_store))
    // Create the directory containing the user data.
    .def("_install", py::make_function(&_User_install))
    // Create the directory containing the user data.
    .def("_signin", &User::signin)
    // Create the directory containing the user data.
    .def("_login", &User::login)

    .add_property("identifier",
                  py::make_function(&User::identifier, by_value()))
    .add_property("description",
                  py::make_function(&User::description, by_value()))

    // Delete the directory containing the user data including his networks.
    .def("_uninstall", py::make_function(&_User_uninstall))
    .staticmethod("_uninstall")

    .def("_erase", py::make_function(&_User_erase))
    .staticmethod("_erase")

    .def("_signout", &User::signout)
    .staticmethod("_signout")

    .def("_store_token", py::make_function(&_User_store_token))
    .staticmethod("_store_token")
  ;


  /*--------.
  | Network |
  `--------*/

  py::enum_<plasma::meta::Client::DescriptorList>("descriptor_list")
    .value("ALL", plasma::meta::Client::DescriptorList::all)
    .value("MINE", plasma::meta::Client::DescriptorList::mine)
    .value("OTHER", plasma::meta::Client::DescriptorList::other)
  ;

  py::class_<Network, boost::noncopyable>("_Network", py::no_init)
    // Constructor with name, user, passphrase, model, policy, opennes.
    .def("__init__", py::make_constructor(&_Network_new))
    // Constructor with descritpor path.
    .def("__init__", py::make_constructor(&_Network_new_from_path))
    // Constructor with name, owner and home.
    .def("__init__", py::make_constructor(&_Network_new_from_home))
    // Store the descriptor to the given path.
    .def("_store", py::make_function(&_Network_store))
    // Create the directory containing the network shelter.
    .def("_install", py::make_function(&_Network_install))
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
                  py::make_function(&Network::everybody_group_identity,
                                    by_value()))
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

    // Delete the given descriptor. (rm descriptor).
    .def("_erase", py::make_function(&_Network_erase_s))
    .staticmethod("_erase")
    // Delete the directory containing the network shelter (rm -r networkpath).
    .def("_uninstall", py::make_function(&_Network_uninstall_s))
    .staticmethod("_uninstall")
    // Unpublish the descriptor from the remote.
    .def("_unpublish", py::make_function(&Network::unpublish))
    .staticmethod("_unpublish")
    // List local descriptor from a given path.
    .def("_list", py::make_function(&_Network_list))
    .staticmethod("_list")
    // List the descriptor stored on the remote.
    .def("_fetch", py::make_function(&_Network_fetch))
    .staticmethod("_fetch")
  ;
}
