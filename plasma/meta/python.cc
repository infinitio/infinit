#include <boost/python.hpp>

#include <plasma/meta/Client.hh>

using namespace boost::python;

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

void export_meta();
void export_meta()
{
  enum_<Error>("Error")
# define ERR_CODE(_name, _value, comment)                                         \
      .value(#_name, Error::_name)
# include <oracle/disciples/meta/src/meta/error_code.hh.inc>
# undef ERR_CODE
  ;

  class_<Transaction>("Transaction");
  class_<Response>("Response")
    .add_property("success", &Response::success)
    .def_readonly("error_details", &Response::error_details)
    .def_readonly("error_code", &Response::error_code)
  ;
  class_<User>("User")
    .def_readonly("id", &User::id)
    .def_readonly("fullname", &User::fullname)
    .def_readonly("handle", &User::handle)
    .def_readonly("public_key", &User::public_key)
    .def_readonly("status", &User::status)
    .def_readonly("connected_devices", &User::connected_devices)
  ;
  class_<Device>("Device")
    .def_readonly("id", &Device::id)
    .def_readonly("name", &Device::name)
    .def_readonly("passport", &Device::passport)
  ;
  class_<DebugResponse, bases<Response>>("DebugResponse");
  class_<LoginResponse, bases<Response>>("LoginResponse")
    .def_readonly("token", &LoginResponse::token)
    .def_readonly("fullname", &LoginResponse::fullname)
    .def_readonly("handle", &LoginResponse::handle)
    .def_readonly("email", &LoginResponse::email)
    .def_readonly("identity", &LoginResponse::identity)
    .def_readonly("id", &LoginResponse::id)
  ;
  class_<LogoutResponse, bases<Response>>("LogoutResponse");
  class_<RegisterResponse, bases<Response>>("RegisterResponse");
  class_<PullNotificationResponse, bases<Response>>("PullNotificationResponse");

  class_<ReadNotificationResponse, bases<Response>>("ReadNotificationResponse");
  class_<MessageResponse, bases<Response>>("MessageResponse");
  class_<UserResponse, bases<User, Response>>("UserResponse");
  class_<SelfResponse, bases<UserResponse>>("SelfResponse")
    .def_readonly("identity", &SelfResponse::identity)
    .def_readonly("email", &SelfResponse::email)
    .def_readonly("remaining_invitations", &SelfResponse::remaining_invitations)
    .def_readonly("token_generation_key", &SelfResponse::token_generation_key)
  ;

  class_<InviteUserResponse, bases<Response>>("InviteUserResponse")
    .def_readonly("_id", &InviteUserResponse::_id)
  ;
  class_<UsersResponse, bases<Response>>("UsersResponse")
    .def_readonly("users", &UsersResponse::users)
  ;
  class_<SwaggersResponse, bases<Response>>("SwaggersResponse")
    .def_readonly("swaggers", &SwaggersResponse::swaggers)
  ;
  class_<TransactionsResponse, bases<Response>>("TransactionsResponse")
    .def_readonly("transaction", &TransactionsResponse::transactions)
  ;
  class_<TransactionResponse, bases<Response, Transaction>>("TransactionResponse");
  class_<CreateTransactionResponse, bases<Response>>("CreateTransactionResponse")
    .def_readonly("created_transaction_id", &CreateTransactionResponse::created_transaction_id)
    .def_readonly("remaining_invitations", &CreateTransactionResponse::remaining_invitations)
  ;

  class_<UpdateTransactionResponse, bases<Response>>("UpdateTransactionResponse")
    .def_readonly("updated_transaction_id", &UpdateTransactionResponse::updated_transaction_id)
  ;

  class_<NetworksResponse, bases<Response>>("NetworksResponse")
    .def_readonly("networks", &NetworksResponse::networks)
  ;

  class_<NetworkResponse, bases<Response>>("NetworkResponse")
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

  class_<CreateNetworkResponse, bases<Response>>("CreateNetworkResponse")
    .def_readonly("created_network_id", &CreateNetworkResponse::created_network_id)
  ;

  class_<DeleteNetworkResponse, bases<Response>>("DeleteNetworkResponse")
    .def_readonly("deleted_network_id", &DeleteNetworkResponse::deleted_network_id)
  ;
  class_<UpdateNetworkResponse, bases<Response>>("UpdateNetworkResponse")
    .def_readonly("updated_network_id", &UpdateNetworkResponse::updated_network_id)
  ;
  class_<NetworkNodesResponse, bases<Response>>("NetworkNodesResponse")
    .def_readonly("network_id", &NetworkNodesResponse::network_id)
    .def_readonly("nodes", &NetworkNodesResponse::nodes)
  ;
  class_<EndpointNodeResponse, bases<Response>>("EndpointNodeResponse")
    .def_readonly("externals", &EndpointNodeResponse::externals)
    .def_readonly("locals", &EndpointNodeResponse::locals)
    .def_readonly("fallback", &EndpointNodeResponse::fallback)
  ;
  class_<CreateDeviceResponse, bases<Response, Device>>("CreateDeviceResponse")
  ;
  class_<UpdateDeviceResponse, bases<Response, Device>>("UpdateDeviceResponse")
  ;

  void                  (Client::*set_token)(std::string const &)       = &Client::token;
  std::string const&    (Client::*get_token)() const                    = &Client::token;
  void                  (Client::*set_email)(std::string const &)       = &Client::email;
  std::string const&    (Client::*get_email)() const                    = &Client::email;
  void                  (Client::*set_identity)(std::string const &)       = &Client::identity;
  std::string const&    (Client::*get_identity)() const                    = &Client::identity;

  class_<Client, boost::noncopyable>(
    "Meta", init<std::string const&, uint16_t>())
    .def("debug", &Client::debug)
    .def("login", &Client::login)
    .def("generate_token", &Client::generate_token)
    .def("user", &Client::user)
    .def("register", &Client::register_)
    //.def("user_icon", &Client::user_icon)
    .def("self", &Client::self)
    .def("user_from_public_key", &Client::user_from_public_key)
    .def("search_users", &Client::search_users)
    .def("get_swaggers", &Client::get_swaggers)
    .def("create_device", &Client::create_device)
    .def("update_device", &Client::update_device)
    .def("invite_user", &Client::invite_user)
    .def("transaction", &Client::transaction)
    .def("transactions", &Client::transactions)
    .def("create_transaction", &Client::create_transaction)
    .def("update_transaction", &Client::update_transaction)
    .def("accept_transaction", &Client::accept_transaction)
    .def("send_message", &Client::send_message)
    .def("pull_notifications", &Client::pull_notifications)
    .def("notification_read", &Client::notification_read)
    .def("network", &Client::network)
    .def("networks", &Client::networks)
    .def("create_network", &Client::create_network)
    .def("delete_network", &Client::delete_network)
    .def("network_nodes", &Client::network_nodes)
    .def("update_network", &Client::update_network)
    .def("network_add_user", &Client::network_add_user)
    .def("network_add_device", &Client::network_add_device)
    //.def("network_connect_device", &Client::network_connect_device)
    .add_property("token",
                  make_function(get_token, return_internal_reference<>()),
                  set_token)
    .add_property("identity",
                  make_function(get_identity, return_internal_reference<>()),
                  set_identity)
    .add_property("email",
                  make_function(get_email, return_internal_reference<>()),
                  set_email)
    ;
}
