#include <boost/python.hpp>

#include <elle/print.hh>
#include <plasma/meta/Client.hh>

template <class T>
struct container_wrap
{
  typedef typename T::value_type value_type;
  typedef typename T::iterator iter_type;

  static
  void add(T & x,
           value_type const& v)
  {
    x.push_back(v);
  }

  static
  bool in(T const& x, value_type const& v)
  {
    return std::find(x.begin(), x.end(), v) != x.end();
  }

  static int index(T const& x, value_type const& v)
  {
    int i = 0;
    for (auto const& val: x)
      if( val == v ) return i;

    PyErr_SetString(PyExc_ValueError, "Value not in the list");
    throw boost::python::error_already_set();
  }

  static void del(T& x, int i)
  {
    if( i<0 )
      i += x.size();

    iter_type it = x.begin();
    for (int pos = 0; pos < i; ++pos)
      ++it;

    if( i >= 0 && i < (int)x.size() ) {
      x.erase(it);
    } else {
      PyErr_SetString(PyExc_IndexError, "Index out of range");
      boost::python::throw_error_already_set();
    }
  }

  static value_type& get(T& x, int i)
  {
    if( i < 0 )
      i += x.size();

    if( i >= 0 && i < (int)x.size() ) {
      iter_type it = x.begin();
      for(int pos = 0; pos < i; ++pos)
        ++it;
      return *it;
    } else {
      PyErr_SetString(PyExc_IndexError, "Index out of range");
      throw boost::python::error_already_set();
    }
  }

  static void set(T& x, int i, value_type const& v)
  {
    if( i < 0 )
      i += x.size();

    if( i >= 0 && i < (int)x.size() ) {
      iter_type it = x.begin();
      for(int pos = 0; pos < i; ++pos)
        ++it;
      *it = v;
    } else {
      PyErr_SetString(PyExc_IndexError, "Index out of range");
      boost::python::throw_error_already_set();
    }
  }
};


template <typename container>
void export_container(const char* typeName)
{
  using namespace boost::python;

  class_<container>(typeName)
    .def("__len__", &container::size)
    .def("clear", &container::clear)
    .def("append", &container_wrap<container>::add, with_custodian_and_ward<1,2>()) // to let container keep value
    .def("__getitem__", &container_wrap<container>::get, return_value_policy<copy_non_const_reference>())
    .def("__setitem__", &container_wrap<container>::set, with_custodian_and_ward<1,2>()) // to let container keep value
    .def("__delitem__", &container_wrap<container>::del)
    .def("__contains__", &container_wrap<container>::in)
    .def("__iter__", iterator<container>())
    .def("index", &container_wrap<container>::index);
}

namespace py = boost::python;

using plasma::meta::Client;
using plasma::Transaction;
using plasma::meta::Error;
using plasma::meta::Response;
using plasma::meta::Exception;
using plasma::meta::DebugResponse;
using plasma::meta::LoginResponse;
using plasma::meta::LogoutResponse;
using plasma::meta::RegisterResponse;
using plasma::meta::PullNotificationResponse;
using plasma::meta::ReadNotificationResponse;
using plasma::meta::MessageResponse;
using plasma::meta::User;
using plasma::meta::UserResponse;
using plasma::meta::SelfResponse;
using plasma::meta::InviteUserResponse;
using plasma::meta::UsersResponse;
using plasma::meta::SwaggersResponse;
using plasma::meta::TransactionsResponse;
using plasma::meta::TransactionResponse;
using plasma::meta::CreateTransactionResponse;
using plasma::meta::UpdateTransactionResponse;
using plasma::meta::NetworksResponse;
using plasma::meta::NetworkResponse;
using plasma::meta::CreateNetworkResponse;
using plasma::meta::DeleteNetworkResponse;
using plasma::meta::UpdateNetworkResponse;
using plasma::meta::NetworkNodesResponse;
using plasma::meta::EndpointNodeResponse;
using plasma::meta::Device;
using plasma::meta::CreateDeviceResponse;
using plasma::meta::UpdateDeviceResponse;
using plasma::meta::AddSwaggerResponse;

class MetaClient:
  public Client
{
public:
  MetaClient(std::string const& host, int port)
    : Client(host, port)
  {
  }

  RegisterResponse
  register_(std::string const& email,
            std::string const& fullname,
            std::string const& password,
            std::string const& activation_code)
  {
    return Client::register_(email, fullname, password, activation_code);
  }
};

struct std_list_to_pylist
{
  static PyObject* convert(std::vector<std::string> const& l)
  {
    boost::python::list pyl;
    for (auto const& elem: l)
    {
      pyl.append(elem);
    }
    return boost::python::incref(pyl.ptr());
  }
};

void export_meta();
void export_meta()
{
  typedef py::return_value_policy<py::return_by_value> by_value;

  py::enum_<Error>("Error")
# define ERR_CODE(_name, _value, comment)                                         \
      .value(#_name, Error::_name)
# include <oracle/disciples/meta/src/meta/error_code.hh.inc>
# undef ERR_CODE
  ;
  export_container<std::vector<std::string>>("vector_string");
  export_container<std::list<std::string>>("list_string");

  py::class_<Transaction>("Transaction");
  py::class_<Response>("Response")
    .add_property("success", &Response::success)
    .def_readonly("error_details", &Response::error_details)
    .def_readonly("error_code", &Response::error_code)
  ;
  py::class_<User>("User")
    .def_readonly("id", &User::id)
    .def_readonly("fullname", &User::fullname)
    .def_readonly("handle", &User::handle)
    .def_readonly("public_key", &User::public_key)
    .def_readonly("status", &User::status)
    .def_readonly("connected_devices", &User::connected_devices)
  ;
  py::class_<Device>("Device")
    .def_readonly("id", &Device::id)
    .def_readonly("name", &Device::name)
    .def_readonly("passport", &Device::passport)
  ;
  py::class_<DebugResponse, py::bases<Response>>("DebugResponse");
  py::class_<LoginResponse, py::bases<Response>>("LoginResponse")
    .def_readonly("token", &LoginResponse::token)
    .def_readonly("fullname", &LoginResponse::fullname)
    .def_readonly("handle", &LoginResponse::handle)
    .def_readonly("email", &LoginResponse::email)
    .def_readonly("identity", &LoginResponse::identity)
    .def_readonly("id", &LoginResponse::id)
  ;
  py::class_<LogoutResponse, py::bases<Response>>("LogoutResponse");
  py::class_<RegisterResponse, py::bases<Response>>("RegisterResponse");
  py::class_<PullNotificationResponse, py::bases<Response>>("PullNotificationResponse");

  py::class_<ReadNotificationResponse, py::bases<Response>>("ReadNotificationResponse");
  py::class_<MessageResponse, py::bases<Response>>("MessageResponse");
  py::class_<UserResponse, py::bases<User, Response>>("UserResponse");
  py::class_<SelfResponse, py::bases<UserResponse>>("SelfResponse")
    .def_readonly("identity", &SelfResponse::identity)
    .def_readonly("email", &SelfResponse::email)
    .def_readonly("remaining_invitations", &SelfResponse::remaining_invitations)
    .def_readonly("token_generation_key", &SelfResponse::token_generation_key)
  ;

  py::class_<InviteUserResponse, py::bases<Response>>("InviteUserResponse")
    .def_readonly("_id", &InviteUserResponse::_id)
  ;
  py::class_<UsersResponse, py::bases<Response>>("UsersResponse")
    .def_readonly("users", &UsersResponse::users)
  ;
  py::class_<SwaggersResponse, py::bases<Response>>("SwaggersResponse")
    .def_readonly("swaggers", &SwaggersResponse::swaggers)
  ;

  py::class_<TransactionsResponse, py::bases<Response>>("TransactionsResponse")
    .def_readonly("transactions", &TransactionsResponse::transactions)
  ;
  py::class_<TransactionResponse, py::bases<Response, Transaction>>("TransactionResponse");
  py::class_<CreateTransactionResponse, py::bases<Response>>("CreateTransactionResponse")
    .def_readonly("created_transaction_id", &CreateTransactionResponse::created_transaction_id)
    .def_readonly("remaining_invitations", &CreateTransactionResponse::remaining_invitations)
  ;

  py::class_<UpdateTransactionResponse, py::bases<Response>>("UpdateTransactionResponse")
    .def_readonly("updated_transaction_id", &UpdateTransactionResponse::updated_transaction_id)
  ;

  py::class_<NetworksResponse, py::bases<Response>>("NetworksResponse")
    .def_readonly("networks", &NetworksResponse::networks)
  ;

  py::class_<NetworkResponse, py::bases<Response>>("NetworkResponse")
    .def_readonly("_id", &NetworkResponse::_id)
    .def_readonly("owner", &NetworkResponse::owner)
    .def_readonly("name", &NetworkResponse::name)
    .def_readonly("model", &NetworkResponse::model)
    .def_readonly("root_block", &NetworkResponse::root_block)
    .def_readonly("root_address", &NetworkResponse::root_address)
    .def_readonly("group_block", &NetworkResponse::group_block)
    .def_readonly("group_address", &NetworkResponse::group_address)
    .def_readonly("descriptor", &NetworkResponse::descriptor)
    .def_readonly("users", &NetworkResponse::users)
  ;

  py::class_<CreateNetworkResponse, py::bases<Response>>("CreateNetworkResponse")
    .def_readonly("created_network_id", &CreateNetworkResponse::created_network_id)
  ;

  py::class_<DeleteNetworkResponse, py::bases<Response>>("DeleteNetworkResponse")
    .def_readonly("deleted_network_id", &DeleteNetworkResponse::deleted_network_id)
  ;
  py::class_<UpdateNetworkResponse, py::bases<Response>>("UpdateNetworkResponse")
    .def_readonly("updated_network_id", &UpdateNetworkResponse::updated_network_id)
  ;
  py::class_<NetworkNodesResponse, py::bases<Response>>("NetworkNodesResponse")
    .def_readonly("network_id", &NetworkNodesResponse::network_id)
    .def_readonly("nodes", &NetworkNodesResponse::nodes)
  ;
  py::class_<EndpointNodeResponse, py::bases<Response>>("EndpointNodeResponse")
    .def_readonly("externals", &EndpointNodeResponse::externals)
    .def_readonly("locals", &EndpointNodeResponse::locals)
    .def_readonly("fallback", &EndpointNodeResponse::fallback)
  ;

  py::class_<AddSwaggerResponse>("AddSwaggerResponse")
    .def_readonly("swag", &AddSwaggerResponse::direction)
  ;

  py::class_<CreateDeviceResponse, py::bases<Response, Device>>("CreateDeviceResponse")
  ;
  py::class_<UpdateDeviceResponse, py::bases<Response, Device>>("UpdateDeviceResponse")
  ;

  void                  (MetaClient::*set_token)(std::string const &)    = &MetaClient::token;
  std::string const&    (MetaClient::*get_token)() const                 = &MetaClient::token;
  void                  (MetaClient::*set_email)(std::string const &)    = &MetaClient::email;
  std::string const&    (MetaClient::*get_email)() const                 = &MetaClient::email;
  void                  (MetaClient::*set_identity)(std::string const &) = &MetaClient::identity;
  std::string const&    (MetaClient::*get_identity)() const              = &MetaClient::identity;

  py::class_<MetaClient, boost::noncopyable>(
    "Meta", py::init<std::string const&, int>())
    .def("debug", &MetaClient::debug)
    .def("login", &MetaClient::login)
    .def("generate_token", &MetaClient::generate_token)
    .def("user", &MetaClient::user)
    .def("register", &MetaClient::register_)
    //.def("user_icon", &MetaClient::user_icon)
    .def("self", &MetaClient::self)
    .def("user_from_public_key", &MetaClient::user_from_public_key)
    .def("search_users", &MetaClient::search_users)
    .def("add_swaggers", &MetaClient::add_swaggers)
    .def("get_swaggers", &MetaClient::get_swaggers)
    .def("create_device", &MetaClient::create_device)
    .def("update_device", &MetaClient::update_device)
    .def("invite_user", &MetaClient::invite_user)
    .def("transaction", &MetaClient::transaction)
    .def("transactions", &MetaClient::transactions)
    .def("create_transaction", &MetaClient::create_transaction)
    .def("update_transaction", &MetaClient::update_transaction)
    .def("accept_transaction", &MetaClient::accept_transaction)
    .def("send_message", &MetaClient::send_message)
    .def("pull_notifications", &MetaClient::pull_notifications)
    .def("notification_read", &MetaClient::notification_read)
    .def("network", &MetaClient::network)
    .def("networks", &MetaClient::networks)
    .def("create_network", &MetaClient::create_network)
    .def("delete_network", &MetaClient::delete_network)
    .def("network_nodes", &MetaClient::network_nodes)
    .def("update_network", &MetaClient::update_network)
    .def("network_add_user", &MetaClient::network_add_user)
    .def("network_add_device", &MetaClient::network_add_device)
    //.def("network_connect_device", &MetaClient::network_connect_device)
    .add_property("token",
                  make_function(get_token, by_value()),
                  set_token)
    .add_property("identity",
                  make_function(get_identity, by_value()),
                  set_identity)
    .add_property("email",
                  make_function(get_email, by_value()),
                  set_email)
    ;
}
