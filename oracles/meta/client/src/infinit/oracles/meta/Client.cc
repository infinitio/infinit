#include <elle/log.hh>
#include <elle/print.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/MapSerializer.hxx>
#include <elle/serialize/SetSerializer.hxx>

#include <infinit/oracles/meta/Client.hh>
#include <infinit/oracles/meta/macro.hh>

#include <reactor/scheduler.hh>

#include <curly/curly.hh>
#include <curly/curly_sched.hh>

#include <version.hh>

#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

ELLE_LOG_COMPONENT("infinit.plasma.meta.Client");

SERIALIZE_RESPONSE(infinit::oracles::meta::DebugResponse, ar, res)
{
  (void) ar;
  (void) res;
}

/*------.
| Users |
`------*/

SERIALIZE_RESPONSE(infinit::oracles::meta::LoginResponse, ar, res)
{
  ar & named("_id", res.id);
  ar & named("fullname", res.fullname);
  ar & named("handle", res.handle);
  ar & named("email", res.email);
  ar & named("identity", res.identity);
  ar & named("device_id", res.device_id);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::LogoutResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(infinit::oracles::meta::RegisterResponse, ar, res)
{
  ar & named("registered_user_id", res.registered_user_id);
  ar & named("invitation_source", res.invitation_source);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::UsersResponse, ar, res)
{
  ar & named("users", res.users);
}

ELLE_SERIALIZE_SIMPLE(infinit::oracles::meta::User, ar, res, version)
{
  enforce(version == 0);
  ar & named("_id", res.id);
  ar & named("public_key", res.public_key);
  ar & named("fullname", res.fullname);
  ar & named("handle", res.handle);
  ar & named("connected_devices", res.connected_devices);

}

SERIALIZE_RESPONSE(infinit::oracles::meta::UserResponse, ar, res)
{
  ar & static_cast<infinit::oracles::meta::User&>(res);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::SelfResponse, ar, res)
{
  ar & static_cast<infinit::oracles::meta::User&>(res);

  ar & named("email", res.email);
  ar & named("identity", res.identity);
  ar & named("devices", res.devices);
  ar & named("favorites", res.favorites);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::SwaggersResponse, ar, res)
{
  ar & named("swaggers", res.swaggers);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::RemoveSwaggerResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(infinit::oracles::meta::InviteUserResponse, ar, res)
{
  ar & named("_id", res._id);
}

// SERIALIZE_RESPONSE(infinit::oracles::meta::InvitedResponse, ar, res)
// {
//   ar & named("emails", res.emails);
// }

/*--------.
| Devices |
`--------*/

SERIALIZE_RESPONSE(infinit::oracles::meta::CreateDeviceResponse, ar, res)
{
  ar & named("_id", res.id);
  ar & named("passport", res.passport);
  ar & named("name", res.name);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::DeviceResponse, ar, res)
{
  ar & named("_id", res.id);
  ar & named("passport", res.passport);
  ar & named("name", res.name);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::UpdateDeviceResponse, ar, res)
{
  ar & named("_id", res.id);
  ar & named("passport", res.passport);
  ar & named("name", res.name);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::DeleteDeviceResponse, ar, res)
{
  ar & named("_id", res.id);
}

/*-------------.
| Transactions |
`-------------*/

SERIALIZE_RESPONSE(infinit::oracles::meta::TransactionResponse, ar, res)
{
  ar & static_cast<infinit::oracles::Transaction&>(res);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::TransactionsResponse, ar, res)
{
  ar & named("transactions", res.transactions);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::CreateTransactionResponse, ar, res)
{
  ar & named("created_transaction_id", res.created_transaction_id);
  ar & named("remaining_invitations",  res.remaining_invitations);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::UpdateTransactionResponse, ar, res)
{
  ar & named("updated_transaction_id", res.updated_transaction_id);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::MessageResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(infinit::oracles::meta::EndpointNodeResponse, ar, res)
{
  ar & named("externals", res.externals);
  ar & named("locals", res.locals);
  ar & named("fallback", res.fallback);
}

SERIALIZE_RESPONSE(infinit::oracles::meta::ConnectDeviceResponse, ar, res)
{
  (void) ar;
  (void) res;
}

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      using elle::sprintf;
      using reactor::http::Method;

      Exception::Exception(Error const& error, std::string const& message)
        : elle::Exception(message)
        , err{error}
      {}

      bool
      Exception::operator ==(Exception const& e) const
      {
        return (this->err == e.err);
      }

      bool
      Exception::operator ==(Error const& error) const
      {
        return (this->err == error);
      }

      bool
      Exception::operator !=(Error const& error) const
      {
        return !(*this == error);
      }

      namespace json = elle::format::json;

      // - Ctor & dtor ----------------------------------------------------------
      Client::Client(std::string const& server,
                     uint16_t port):
        _host(server),
        _port(port),
        _root_url{elle::sprintf("http://%s:%d", server, port)},
        _client{},
        _default_configuration{}, // Default v1.1, timeout 30sec.
        _email{},
        _user_agent{"MetaClient/" INFINIT_VERSION}
      {
        ELLE_LOG("%s: creating a meta client", this);
        //ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      }

      Client::~Client()
      {
      }

      // - API calls ------------------------------------------------------------

      DebugResponse
      Client::status() const
      {
        return this->_request<DebugResponse>("/status", Method::GET);
      }

      /*------.
      | Users |
      `------*/
      LoginResponse
      Client::login(std::string const& email,
                    std::string const& password,
                    boost::uuids::uuid const& device_uuid)
      {
        std::string struuid = boost::lexical_cast<std::string>(device_uuid);

        json::Dictionary request{
          std::map<std::string, std::string>{
            {"email", email},
            {"password", password},
            {"device_uid", struuid},
         }};

        auto res = this->_request<LoginResponse>(
          "/user/login", Method::POST, request);
        if (res.success())
        {
          // Debugging purpose.
          this->_email = email;
        }
        return res;
      }

      LogoutResponse
      Client::logout()
      {
        auto res = this->_request<LogoutResponse>("/user/logout", Method::GET);
        if (res.success())
        {
          // Debugging purpose.
          this->_email = "";
        }
        return res;
      }

      RegisterResponse
      Client::register_(std::string const& email,
                        std::string const& fullname,
                        std::string const& password,
                        std::string const& activation_code) const
      {
        json::Dictionary request{std::map<std::string, std::string>{
            {"email", email},
            {"fullname", fullname},
            {"password", password},
            {"activation_code", activation_code},
              }};
        return this->_request<RegisterResponse>(
          "/user/register", Method::POST, request);
      }

      UsersResponse
      Client::search_users(std::string const& text, int count, int offset) const
      {
        json::Dictionary request;
        request["text"] = text;
        request["limit"] = count;
        request["offset"] = offset;
        return this->_request<UsersResponse>(
          "/user/search", Method::POST, request);
      }

      UserResponse
      Client::user(std::string const& id) const
      {
        if (id.size() == 0)
          throw elle::Exception("Wrong id");
        return this->_request<UserResponse>(
          sprintf("/user/%s/view", id), Method::GET);
      }

      SelfResponse
      Client::self() const
      {
        return this->_request<SelfResponse>("/self", Method::GET);
      }

      UserResponse
      Client::user_from_public_key(std::string const& public_key) const
      {
        if (public_key.size() == 0)
          throw elle::Exception("empty public key!");
        json::Dictionary request;
        request["public_key"] = public_key;
        return this->_request<UserResponse>(
          "/user/from_public_key", Method::POST, request);
      }

      SwaggersResponse
      Client::get_swaggers() const
      {
        return this->_request<SwaggersResponse>("/user/swaggers", Method::GET);
      }

      RemoveSwaggerResponse
      Client::remove_swagger(std::string const& _id) const
      {
        json::Dictionary request{std::map<std::string, std::string>{
            {"_id", _id},
              }};

        return this->_request<RemoveSwaggerResponse>(
          "/user/remove_swagger", Method::POST, request);
      }

      Response
      Client::favorite(std::string const& user) const
      {
        json::Dictionary request;
        request["user_id"] = user;
        return this->_request<DebugResponse>(
          "/user/favorite", Method::POST, request);
      }

      Response
      Client::unfavorite(std::string const& user) const
      {
        json::Dictionary request;
        request["user_id"] = user;
        return this->_request<DebugResponse>(
          "/user/unfavorite", Method::POST, request);
      }

      // InvitedResponse
      // Client::invited() const
      // {
      //   return this->_get<InvitedResponse>("/user/invited");
      // }

      /*--------.
      | Devices |
      `--------*/
      CreateDeviceResponse
      Client::create_device(std::string const& name) const
      {
        json::Dictionary request{std::map<std::string, std::string>{
            {"name", name},
              }};
        return this->_request<CreateDeviceResponse>(
          "/device/create", Method::POST, request);
      }

      UpdateDeviceResponse
      Client::update_device(std::string const& _id,
                            std::string const& name) const
      {
        json::Dictionary request{std::map<std::string, std::string>{
            {"_id", _id},
            {"name", name},
              }};

        return this->_request<UpdateDeviceResponse>(
          "/device/update", Method::POST, request);
      }

      DeviceResponse
      Client::device(std::string const& _id) const
      {
        json::Dictionary request{std::map<std::string, std::string>{
            {"_id", _id},
              }};

        return this->_request<DeviceResponse>(
          sprintf("/device/%s/update", _id), Method::GET);
      }

      DeleteDeviceResponse
      Client::delete_device(std::string const& _id) const
      {
        json::Dictionary request{std::map<std::string, std::string>{
            {"_id", _id},
              }};

        return this->_request<DeleteDeviceResponse>(
          "/device/delete", Method::POST, request);
      }

      /*------.
      | Users |
      `------*/
      InviteUserResponse
      Client::invite_user(std::string const& email) const
      {
        json::Dictionary request{std::map<std::string, std::string>
          {
            {"email", email}
          }};

        auto res = this->_request<InviteUserResponse>(
          "/user/invite", Method::POST, request);

        return res;
      }

      CreateTransactionResponse
      Client::create_transaction(std::string const& recipient_id_or_email,
                                 std::list<std::string> const& files,
                                 size_t count,
                                 size_t size,
                                 bool is_dir,
                                 std::string const& device_id,
                                 std::string const& message) const
      {
        json::Dictionary request;

        request["recipient_id_or_email"] = recipient_id_or_email;
        request["files"] = files;
        request["device_id"] = device_id;
        request["total_size"] = size;
        request["is_directory"] = is_dir;
        request["files_count"] = count;
        request["message"] = message;

        return this->_request<CreateTransactionResponse>(
          "/transaction/create", Method::POST, request);
      }

      UpdateTransactionResponse
      Client::update_transaction(std::string const& transaction_id,
                                 Transaction::Status status,
                                 std::string const& device_id,
                                 std::string const& device_name) const
      {
        ELLE_TRACE("%s: update %s transaction with new status %s",
                   *this,
                   transaction_id,
                   status);
        json::Dictionary request{};
        request["transaction_id"] = transaction_id;
        request["status"] = (int) status;

        if (status == oracles::Transaction::Status::accepted)
        {
          ELLE_ASSERT_GT(device_id.length(), 0u);
          ELLE_ASSERT_GT(device_name.length(), 0u);
          request["device_id"] = device_id;
          request["device_name"] = device_name;
        }

        return this->_request<UpdateTransactionResponse>(
          "/transaction/update", Method::POST, request);
      }

      UpdateTransactionResponse
      Client::accept_transaction(std::string const& transaction_id,
                                 std::string const& device_id,
                                 std::string const& device_name) const
      {
        ELLE_TRACE("%s: accept %s transaction on device %s (%s)",
                   *this, transaction_id, device_name, device_id);

        return this->update_transaction(transaction_id,
                                        Transaction::Status::accepted,
                                        device_id,
                                        device_name);
      }

      TransactionResponse
      Client::transaction(std::string const& _id) const
      {
        return this->_request<TransactionResponse>(
          sprintf("/transaction/%s/view", _id), Method::GET);
      }

      TransactionsResponse
      Client::transactions(std::vector<Transaction::Status> const& status,
                           bool inclusive,
                           int count) const
      {
        json::Dictionary request{};
        if (status.size() > 0)
        {
          json::Array status_array;
          for (Transaction::Status statu: status)
          {
            status_array.push_back((int) statu);
          }

          request["filter"] = std::move(status_array);
          request["type"] = inclusive;
          request["count"] = count;
        }

        return this->_request<TransactionsResponse>(
          "/transactions", Method::POST, request);
      }

      MessageResponse
      Client::send_message(std::string const& recipient_id,
                           std::string const& sender_id,
                           std::string const& message) const
      {
        json::Dictionary request{std::map<std::string, std::string>
          {
            {"recipient_id", recipient_id},
            {"sender_id", sender_id},
            {"message", message},
              }};

        // FIXME: Time.h ????
        request["time"] = 0;
        request["notification_id"] = 217;

        // FIXME: /user/message
        auto res = this->_request<MessageResponse>(
          "/debug", Method::POST, request);

        return res;
      }

      DebugResponse
      Client::debug() const
      {
        return this->_request<DebugResponse>("/scratchit", Method::GET);
      }

      ConnectDeviceResponse
      Client::connect_device(std::string const& transaction_id,
                             std::string const& device_id,
                             adapter_type const& local_endpoints,
                             adapter_type const& public_endpoints) const
      {
        json::Dictionary request{
          std::map<std::string, std::string>{
            {"_id", transaction_id},
            {"device_id", device_id},
              }
        };

        json::Array local_addrs;
        for (auto& a: local_endpoints)
        {
          json::Dictionary endpoint;

          endpoint["ip"] = a.first;
          endpoint["port"] = a.second;
          local_addrs.push_back(endpoint);
        }

        request["locals"] = local_addrs;

        json::Array public_addrs;
        for (auto& a : public_endpoints)
        {
          json::Dictionary pub_addr;

          pub_addr["ip"] = a.first;
          pub_addr["port"] = a.second;
          public_addrs.push_back(pub_addr);
        }

        request["externals"] = public_addrs;

        return this->_request<ConnectDeviceResponse>(
          "/transaction/connect_device", Method::POST, request);
      }

      EndpointNodeResponse
      Client::device_endpoints(std::string const& network_id,
                               std::string const& self_device_id,
                               std::string const& device_id) const
      {
        json::Dictionary request{
          std::map<std::string, std::string>{
            {"self_device_id", self_device_id},
            {"device_id", device_id},
              }
        };

        return this->_request<EndpointNodeResponse>(
          sprintf("/transaction/%s/endpoints", network_id),
          Method::POST, request);
      }

      //- Properties ------------------------------------------------------------

      std::ostream&
      operator <<(std::ostream& out,
                  Error e)
      {
        switch (e)
        {
#define ERR_CODE(name, value, comment)                                          \
          case Error::name:                                                     \
            out << #name << "(" << #comment << ")";                             \
            break;
#include <oracle/disciples/meta/src/meta/error_code.hh.inc>
#undef ERROR_CODE
        }

        return out;
      }

      /*----------.
      | Printable |
      `----------*/
      void
      Client::print(std::ostream& stream) const
      {
        stream << "meta::Client(" << this->_host << ":" << this->_port << " @" << this->_email << ")";
      }
    }
  }
}
