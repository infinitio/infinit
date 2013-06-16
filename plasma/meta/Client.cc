#include "Client.hh"

#include <infinit/Identity.hh>

#include <elle/log.hh>
#include <elle/print.hh>
#include <elle/serialize/JSONArchive.hh>
#include <elle/format/json/Dictionary.hxx>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/MapSerializer.hxx>
#include <elle/serialize/extract.hh>
#include <elle/serialize/Base64Archive.hh>

#include <curly/curly.hh>

#include <fstream>

ELLE_LOG_COMPONENT("infinit.plasma.meta.Client");

// - API responses serializers ------------------------------------------------
#define SERIALIZE_RESPONSE(type, archive, value)                              \
  ELLE_SERIALIZE_NO_FORMAT(type);                                             \
  ELLE_SERIALIZE_SIMPLE(type, archive, value, version)                        \
  {                                                                           \
    enforce(version == 0);                                                    \
    archive & named("success", value._success);                               \
    if (!value.success())                                                     \
    {                                                                         \
      int* n = (int*) &value.error_code;                                      \
      archive & named("error_code", *n);                                      \
      archive & named("error_details", value.error_details);                  \
      return;                                                                 \
    }                                                                         \
    ResponseSerializer<type>::serialize(archive, value);                      \
  }                                                                           \
  template<> template<typename Archive, typename Value>                       \
  void elle::serialize::ResponseSerializer<type>::serialize(Archive& archive, \
                                                            Value& value)     \
/**/

namespace elle
{
  namespace serialize
  {
    template<typename T>
    struct ResponseSerializer
    {
      ELLE_SERIALIZE_BASE_CLASS_MIXIN_TN(T, 0)

      template<typename Archive, typename Value>
      static void serialize(Archive&, Value&);
    };
  }
}

SERIALIZE_RESPONSE(plasma::meta::DebugResponse, ar, res)
{
  (void) ar;
  (void) res;
}


SERIALIZE_RESPONSE(plasma::meta::LoginResponse, ar, res)
{
  ar & named("token", res.token);
  ar & named("fullname", res.fullname);
  ar & named("email", res.email);
  ar & named("identity", res.identity);
  ar & named("_id", res.id);
}

SERIALIZE_RESPONSE(plasma::meta::LogoutResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(plasma::meta::RegisterResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(plasma::meta::UserResponse, ar, res)
{
  ar & named("_id", res.id);
  ar & named("fullname", res.fullname);
  ar & named("handle", res.handle);
  ar & named("public_key", res.public_key);
  ar & named("status", res.status);
  ar & named("connected_devices", res.connected_devices);
}

SERIALIZE_RESPONSE(plasma::meta::SelfResponse, ar, res)
{
  ar & named("_id", res.id);
  ar & named("fullname", res.fullname);
  ar & named("handle", res.handle);
  ar & named("email", res.email);
  ar & named("public_key", res.public_key);
  ar & named("identity", res.identity);
  ar & named("remaining_invitations",  res.remaining_invitations);
  ar & named("status", res.status);
  try
  {
    ar & named("token_generation_key", res.token_generation_key);
  }
  catch (std::exception const& e)
  {
    ELLE_WARN("User{%s, %s, %s}, has no member token_generation_key",
              res.fullname, res.id, res.email);
    res.token_generation_key = "";
  }
}

SERIALIZE_RESPONSE(plasma::meta::UsersResponse, ar, res)
{
  ar & named("users", res.users);
}

SERIALIZE_RESPONSE(plasma::meta::SwaggersResponse, ar, res)
{
  ar & named("swaggers", res.swaggers);
}

SERIALIZE_RESPONSE(plasma::meta::CreateDeviceResponse, ar, res)
{
  ar & named("created_device_id", res.id);
  ar & named("passport", res.passport);
  // XXXX
  // ar & named("name", res.name);
  res.name = "Todo";
}

SERIALIZE_RESPONSE(plasma::meta::UpdateDeviceResponse, ar, res)
{
  ar & named("updated_device_id", res.id);
  ar & named("passport", res.passport);
  // XXXX
  // ar & named("name", res.name);
  res.name = "Todo";
}

SERIALIZE_RESPONSE(plasma::meta::InviteUserResponse, ar, res)
{
  ar & named("_id", res._id);
}

SERIALIZE_RESPONSE(plasma::meta::TransactionResponse, ar, res)
{
  ar & static_cast<plasma::Transaction&>(res);
}

SERIALIZE_RESPONSE(plasma::meta::TransactionsResponse, ar, res)
{
  ar & named("transactions", res.transactions);
}

SERIALIZE_RESPONSE(plasma::meta::CreateTransactionResponse, ar, res)
{
  ar & named("created_transaction_id", res.created_transaction_id);
  ar & named("remaining_invitations",  res.remaining_invitations);
}

SERIALIZE_RESPONSE(plasma::meta::UpdateTransactionResponse, ar, res)
{
  ar & named("updated_transaction_id", res.updated_transaction_id);
}

SERIALIZE_RESPONSE(plasma::meta::MessageResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(plasma::meta::PullNotificationResponse, ar, res)
{
  ar & named("notifs", res.notifs);
  ar & named("old_notifs", res.old_notifs);
}

SERIALIZE_RESPONSE(plasma::meta::ReadNotificationResponse, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(plasma::meta::NetworksResponse, ar, res)
{
  ar & named("networks", res.networks);
}

SERIALIZE_RESPONSE(plasma::meta::NetworkNodesResponse, ar, res)
{
  ar & named("network_id", res.network_id);
  ar & named("nodes", res.nodes);
}

SERIALIZE_RESPONSE(plasma::meta::EndpointNodeResponse, ar, res)
{
  ar & named("externals", res.externals);
  ar & named("locals", res.locals);
  ar & named("fallback", res.fallback);
}

SERIALIZE_RESPONSE(plasma::meta::CreateNetworkResponse, ar, res)
{
  ar & named("created_network_id", res.created_network_id);
}

SERIALIZE_RESPONSE(plasma::meta::DeleteNetworkResponse, ar, res)
{
  ar & named("deleted_network_id", res.deleted_network_id);
}

SERIALIZE_RESPONSE(plasma::meta::UpdateNetworkResponse, ar, res)
{
  ar & named("updated_network_id", res.updated_network_id);
}

SERIALIZE_RESPONSE(plasma::meta::NetworkResponse, ar, res)
{
  ar & named("_id", res._id);
  ar & named("name", res.name);
  ar & named("owner", res.owner);
  ar & named("model", res.model);
  try
    {
      ar & named("root_block", res.root_block);
      ar & named("root_address", res.root_address);
      ar & named("group_block", res.group_block);
      ar & named("group_address", res.group_address);
      ar & named("descriptor", res.descriptor);
    }
  catch (std::exception const&)
    {
      if (Archive::mode != ArchiveMode::Input)
          throw;
      res.root_block = "";
      res.root_address = "";
      res.group_block = "";
      res.group_address = "";
      res.descriptor = "";
    }
  ar & named("users", res.users);
}

SERIALIZE_RESPONSE(plasma::meta::SignHashResponse, ar, res)
{
  ar & named("signature", res.signature);
}

SERIALIZE_RESPONSE(plasma::meta::VerifySignatureResponse, ar, res)
{
  ar & named("verified", res.verified);
}

SERIALIZE_RESPONSE(plasma::meta::PublishDescriptorResponse, ar, res)
{
  ar & named("_id", res.id);
}

SERIALIZE_RESPONSE(plasma::meta::UnpublishDescriptorResponse, ar, res)
{
  ar & named("_id", res.id);
}

ELLE_SERIALIZE_SIMPLE(plasma::meta::Descriptor, ar, res, version)
{
  (void) version;

  ar & named("_id", res.id);
  ar & named("descriptor", res.descriptor);
}

SERIALIZE_RESPONSE(plasma::meta::DescriptorResponse, ar, res)
{
  ar & base_class<plasma::meta::Descriptor>(res);
}

SERIALIZE_RESPONSE(plasma::meta::DescriptorsResponse, ar, res)
{
  ar & named("descriptors", res.descriptors);
}

SERIALIZE_RESPONSE(plasma::meta::DescriptorListResponse, ar, res)
{
  ar & named("descriptors", res.descriptors);
}

namespace plasma
{
  namespace meta
  {

    Exception::Exception(Error const& error, std::string const& message)
      : std::runtime_error(message)
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

    namespace json = elle::format::json;

    static
    std::string
    token_from_file(boost::filesystem::path const& tokenpath)
    {
        std::string _token_genkey;
        if (!tokenpath.empty() && boost::filesystem::exists(tokenpath))
        {
          ELLE_DEBUG("read generation token from %s", tokenpath);
          std::ifstream token_file{tokenpath.string()};

          std::getline(token_file, _token_genkey);
        }

        return _token_genkey;
    }

     // - Ctor & dtor ----------------------------------------------------------
    Client::Client(std::string const& server,
                   uint16_t port,
                   bool check_errors,
                   std::string const& token_path):
      _root_url{elle::sprintf("http://%s:%d", server, port)},
      _check_errors{check_errors},
      _identity{},
      _email{},
      _token{},
      _user_agent{"MetaClient"}
    {
      if (!token_path.empty())
        this->generate_token(token_path);
    }

    Client::Client(std::string const& server,
                   uint16_t port,
                   bool check_errors,
                   boost::filesystem::path const& token_path):
      Client(server, port, check_errors, token_from_file(token_path))
    {}

    Client::~Client()
    {}

    // - API calls ------------------------------------------------------------
    // XXX add login with token method.
    LoginResponse
    Client::login(std::string const& email,
                  std::string const& password)
    {
      json::Dictionary request{std::map<std::string, std::string>{
        {"email", email},
        {"password", password},
      }};
      auto res = this->_post<LoginResponse>("/user/login", request);
      if (res.success())
        {
          this->_token = res.token;
          this->_identity = res.identity;
          this->_email = email;

          auto extractor =
            elle::serialize::from_string<
              elle::serialize::InputBase64Archive>(res.identity);
          infinit::Identity identity(extractor);
          this->_keypair.reset(
            new cryptography::KeyPair(identity.decrypt(password)));
        }
      return res;
    }

    LoginResponse
    Client::generate_token(std::string const& token_genkey)
    {
      json::Dictionary request;
      request["token_generation_key"] = token_genkey;

      auto res = this->_post<LoginResponse>("/user/generate_token", request);
      if (res.success())
      {
        this->_token = res.token;
        this->_identity = res.identity;
        this->_email = res.email;
      }
      return res;
    }

    LogoutResponse
    Client::logout()
    {
      auto res = this->_get<LogoutResponse>("/user/logout");
      if (res.success())
        {
          this->_token = "";
          this->_identity = "";
          this->_email = "";
        }
      return res;
    }

    RegisterResponse
    Client::register_(std::string const& email,
                      std::string const& fullname,
                      std::string const& password,
                      std::string const& activation_code,
                      std::string const& picture_name,
                      std::string const& picture_data) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
        {"email", email},
        {"fullname", fullname},
        {"password", password},
        {"activation_code", activation_code},
        {"picture_name", picture_name},
        {"picture_data", picture_data},
      }};
      return this->_post<RegisterResponse>("/user/register", request);
    }

    UserResponse
    Client::user(std::string const& id) const
    {
      if (id.size() == 0)
        throw std::runtime_error("Wrong id");
      return this->_get<UserResponse>("/user/" + id + "/view");
    }

    UserIcon
    Client::user_icon(std::string const& id) const
    {
      std::stringstream resp;
      curly::request_configuration c = curly::make_get();

      c.option(CURLOPT_DEBUGFUNCTION, curl_debug_callback);
      c.option(CURLOPT_DEBUGDATA, nullptr);

      c.url(elle::sprintf("%s/user/%s/icon", this->_root_url, id));
      c.output(resp);
      c.option(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
      c.user_agent(this->_user_agent);
      c.headers({
        {"Authorization", this->_token},
        {"Connection", "close"},
      });
      curly::request request(std::move(c));
      std::string sdata = resp.str();
      return elle::Buffer{(elle::Byte const*)sdata.data(), sdata.size()};
    }

    SelfResponse
    Client::self() const
    {
      return this->_get<SelfResponse>("/self");
    }

    UserResponse
    Client::user_from_public_key(std::string const& public_key) const
    {
      if (public_key.size() == 0)
        throw std::runtime_error("empty public key!");
      json::Dictionary request;
      request["public_key"] = public_key;
      return this->_post<UserResponse>("/user/from_public_key", request);
    }

    UsersResponse
    Client::search_users(std::string const& text, int count, int offset) const
    {
      json::Dictionary request;
      request["text"] = text;
      request["count"] = count;
      request["offset"] = offset;
      return this->_post<UsersResponse>("/user/search", request);
    }

    SwaggersResponse
    Client::get_swaggers() const
    {
      return this->_get<SwaggersResponse>("/user/swaggers");
    }

    // SwaggerResponse
    // Client::get_swagger(std::string const& id) const
    // {
    //   return this->_get<SwaggerResponse>("/user/" + id + "/view");
    // }


    //- Devices ---------------------------------------------------------------

    CreateDeviceResponse
    Client::create_device(std::string const& name) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"name", name},
      }};
      return this->_post<CreateDeviceResponse>("/device/create", request);
    }

    UpdateDeviceResponse
    Client::update_device(std::string const& _id,
                          std::string const& name) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
            {"_id", _id},
            {"name", name},
      }};

      return this->_post<UpdateDeviceResponse>("/device/update", request);

    }

    InviteUserResponse
    Client::invite_user(std::string const& email) const
    {
      json::Dictionary request{std::map<std::string, std::string>
        {
          {"email", email}
        }};

      auto res = this->_post<InviteUserResponse>("/user/invite", request);

      return res;
    }

    CreateTransactionResponse
    Client::create_transaction(std::string const& recipient_id_or_email,
                               std::string const& first_filename,
                               size_t count,
                               size_t size,
                               bool is_dir,
                               std::string const& network_id,
                               std::string const& device_id) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"recipient_id_or_email", recipient_id_or_email},
          {"first_filename", first_filename},
          {"device_id", device_id},
          {"network_id", network_id},
      }};
      request["total_size"] = size;
      request["is_directory"] = is_dir;
      request["files_count"] = count;

      auto res = this->_post<CreateTransactionResponse>("/transaction/create", request);

      return res;
    }

    UpdateTransactionResponse
    Client::update_transaction(std::string const& transaction_id,
                               plasma::TransactionStatus status) const
    {
      ELLE_TRACE("update %s transaction with new status %s",
                 transaction_id,
                 status);
      json::Dictionary request{};
      request["transaction_id"] = transaction_id;
      request["status"] = (int) status;

      return this->_post<UpdateTransactionResponse>("/transaction/update",
                                                    request);
    }

    UpdateTransactionResponse
    Client::accept_transaction(std::string const& transaction_id,
                               std::string const& device_id,
                               std::string const& device_name) const
    {
      ELLE_TRACE("accept %s transaction on device %s (%s)",
                 transaction_id,
                 device_name,
                 device_id);
      json::Dictionary request{};
      request["transaction_id"] = transaction_id;
      request["device_id"] = device_id;
      request["device_name"] = device_name;

      return this->_post<UpdateTransactionResponse>("/transaction/accept",
                                                    request);
    }

    TransactionResponse
    Client::transaction(std::string const& _id) const
    {
      return this->_get<TransactionResponse>("/transaction/" + _id + "/view");
    }

    TransactionsResponse
    Client::transactions() const
    {
      return this->_get<TransactionsResponse>("/transactions");
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
      auto res = this->_post<MessageResponse>("/debug", request);

      return res;
    }

    DebugResponse
    Client::debug() const
    {
      return this->_get<DebugResponse>("/scratchit");
    }

    PullNotificationResponse
    Client::pull_notifications(int count, int offset) const
    {
      json::Dictionary request{std::map<std::string, std::string>
      {
      }};

      request["count"] = count;
      request["offset"] = offset;

      auto res = this->_post<PullNotificationResponse>("/notification/get",
                                                              request);

      return res;
    }

    ReadNotificationResponse
    Client::notification_read() const
    {
      return this->_get<ReadNotificationResponse>("/notification/read");
    }

    //- Networks --------------------------------------------------------------

    NetworksResponse
    Client::networks() const
    {
      return this->_get<NetworksResponse>("/networks");
    }

    NetworkResponse
    Client::network(std::string const& _id) const
    {
      return this->_get<NetworkResponse>("/network/" + _id + "/view");
    }

    NetworkNodesResponse
    Client::network_nodes(std::string const& _id) const
    {
      return this->_get<NetworkNodesResponse>("/network/" + _id + "/nodes");
    }

    CreateNetworkResponse
    Client::create_network(std::string const& network_id) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"name", network_id},
      }};
      return this->_post<CreateNetworkResponse>("/network/create", request);
    }

    SignHashResponse
    Client::sign_hash(std::string const& hash) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"hash", hash},
      }};
      return this->_post<SignHashResponse>("/authority/sign", request);
    }

    VerifySignatureResponse
    Client::verify_signature(std::string const& signature,
                             std::string const& hash) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"hash", hash},
          {"signature", signature},
      }};
      return this->_post<VerifySignatureResponse>("/authority/verify", request);
    }

    PublishDescriptorResponse
    Client::descriptor_publish(std::string const& dsc) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"descriptor", dsc},
      }};
      return this->_post<PublishDescriptorResponse>("/descriptor/publish", request);
    }

    UnpublishDescriptorResponse
    Client::descriptor_unpublish(std::string const& id) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"id", id},
      }};
      return this->_post<UnpublishDescriptorResponse>("/descriptor/unpublish", request);
    }

    DescriptorResponse
    Client::descriptor(std::string const& id) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"id", id},
      }};

      return this->_post<DescriptorResponse>("/descriptor/get", request);
    }

    DescriptorsResponse
    Client::descriptors(Client::DescriptorList filter) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"filter", std::to_string((int) filter)}
      }};

      return this->_get<DescriptorsResponse>("/descriptor/all");
    }

    DescriptorListResponse
    Client::descriptor_list(Client::DescriptorList filter) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"filter", std::to_string((int) filter)}
      }};
      return this->_post<DescriptorListResponse>("/descriptor/list", request);
    }

    DeleteNetworkResponse
    Client::delete_network(std::string const& network_id,
                           bool force) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"network_id", network_id},
      }};
      request["force"] = force;
      return this->_post<DeleteNetworkResponse>("/network/delete", request);
    }

    UpdateNetworkResponse
    Client::update_network(std::string const& _id,
                           std::string const* name,
                           std::string const* root_block,
                           std::string const* root_address,
                           std::string const* group_block,
                           std::string const* group_address) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
            {"_id", _id},
      }};
      if (name != nullptr)
        request["name"] = *name;
        assert(((root_block == nullptr && root_address == nullptr) ||
              (root_block != nullptr && root_address != nullptr)) &&
             "Give both root block and root address or none of them");
      if (root_block != nullptr)
        request["root_block"] = *root_block;
      if (root_address != nullptr)
        request["root_address"] = *root_address;

      assert(((group_block == nullptr && group_address == nullptr) ||
              (group_block != nullptr && group_address != nullptr)) &&
             "Give both group block and group address or none of them");
      if (group_block != nullptr)
        request["group_block"] = *group_block;
      if (group_address != nullptr)
        request["group_address"] = *group_address;

      assert((
        (root_block == nullptr &&
         group_block == nullptr) ||
        (root_block != nullptr &&
         group_block != nullptr)
      ) && "root and group block are tied together.");

      return this->_post<UpdateNetworkResponse>("/network/update", request);
    }

    NetworkAddUserResponse
    Client::network_add_user(std::string const& network_id,
                             std::string const& user_id) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"_id", network_id},
          {"user_id", user_id},
      }};
      return this->_post<NetworkAddUserResponse>("/network/add_user", request);
    }
    NetworkAddDeviceResponse
    Client::network_add_device(std::string const& network_id,
                               std::string const& device_id) const
    {
      json::Dictionary request{std::map<std::string, std::string>{
          {"_id", network_id},
          {"device_id", device_id},
      }};
      return this->_post<NetworkAddDeviceResponse>("/network/add_device", request);
    }

    NetworkConnectDeviceResponse
    Client::network_connect_device(std::string const& network_id,
                                   std::string const& device_id,
                                   std::string const* local_ip,
                                   uint16_t local_port,
                                   std::string const* external_ip,
                                   uint16_t external_port) const
    {
        adapter_type local_adapter;
        adapter_type public_adapter;

        local_adapter.emplace_back(*local_ip, local_port);
        public_adapter.emplace_back(*external_ip, external_port);

        return this->_network_connect_device(network_id,
                                             device_id,
                                             local_adapter,
                                             public_adapter);
    }

    NetworkConnectDeviceResponse
    Client::_network_connect_device(std::string const& network_id,
                                    std::string const& device_id,
                                    adapter_type const& local_endpoints,
                                    adapter_type const& public_endpoints) const
      {
        json::Dictionary request{
          std::map<std::string, std::string>{
                {"_id", network_id},
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

        return this->_post<NetworkConnectDeviceResponse>(
            "/network/connect_device",
            request
        );
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

        return this->_post<EndpointNodeResponse>(
            "/network/" + network_id + "/endpoints",
            request
        );
      }

    //- Properties ------------------------------------------------------------

    void
    Client::token(std::string const& tok)
    {
      this->_token = tok;
    }

    std::string const&
    Client::token() const
    {
      return this->_token;
    }

    std::string const&
    Client::identity() const
    {
      return _identity;
    }

    void
    Client::identity(std::string const& str)
    {
      _identity = str;
    }

    std::string const&
    Client::email() const
    {
      return _email;
    }

    void
    Client::email(std::string const& str)
    {
      _email = str;
    }

    cryptography::KeyPair const&
    Client::keypair() const
    {
      ELLE_ASSERT_NEQ(this->_keypair, nullptr);

      return (*this->_keypair);
    }

    std::ostream&
    operator <<(std::ostream& out,
                Error e)
    {
      switch (e)
      {
#define ERR_CODE(name, value, comment)                 \
        case Error::name:                              \
          out << #name << "(" << #comment << ")";      \
          break;
#include <oracle/disciples/meta/error_code.hh.inc>
#undef ERROR_CODE
      }

      return out;
    }
  }
}
