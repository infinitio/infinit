#ifndef  PLASMA_META_CLIENT_HH
# define PLASMA_META_CLIENT_HH

# include <plasma/plasma.hh>

# include <elle/Exception.hh>
# include <elle/HttpClient.hh>
# include <elle/format/json/fwd.hh>
# include <elle/log.hh>

# include <functional>
# include <list>
# include <map>
# include <memory>
# include <stdexcept>
# include <string>
# include <vector>

namespace plasma
{
  namespace meta
  {
    namespace json = elle::format::json;

    enum class Error: int
    {
# define ERR_CODE(name, value, comment)                                         \
      name = value,
# include <oracle/disciples/meta/src/meta/error_code.hh.inc>
# undef ERR_CODE
    };

    /// Base class for every response
    struct Response
    {
      bool _success;
      Error error_code;
      std::string error_details;

      bool success() const
      {
        return _success;
      }
    };

    struct Exception
      : public elle::Exception
    {
      Error const err;
      Exception(Error const& error, std::string const& message = "");

    public:
      bool operator ==(Exception const& e) const;
      bool operator ==(Error const& error) const;
      bool operator !=(Error const& error) const;
    };

    /////////////////////////
    struct DebugResponse : Response
    {};

    struct LoginResponse : Response
    {
      std::string token;
      std::string fullname;
      std::string handle;
      std::string email;
      std::string identity;
      std::string id;
    };

    struct LogoutResponse : Response
    {};

    struct RegisterResponse:
      public Response
    {
      std::string registered_user_id;
      std::string invitation_source;
    };

    struct PullNotificationResponse : Response
    {
      std::list<json::Dictionary> notifs;
      std::list<json::Dictionary> old_notifs;
    };

    struct ReadNotificationResponse : Response
    {};

    struct MessageResponse : Response
    {};

    struct User
    {
      std::string id;
      std::string fullname;
      std::string handle;
      std::string public_key;
      std::vector<std::string> connected_devices;

      bool
      status() const
      {
        return !this->connected_devices.empty();
      }
    };

    struct UserResponse : User, Response
    {};

    struct SelfResponse : UserResponse
    {
      std::string identity;
      std::string email;
      int remaining_invitations;
      std::string token_generation_key;
      std::list<std::string> devices;
      std::list<std::string> favorites;
    };

    struct InviteUserResponse : Response
    {
      std::string _id;
    };

    struct UsersResponse : Response
    {
      std::list<std::string> users;
    };

    struct AddSwaggerResponse : Response
    {
      std::string direction;
    };

    struct SwaggersResponse : Response
    {
      std::list<std::string> swaggers;
    };

    struct TransactionsResponse : Response
    {
      std::list<std::string> transactions;
    };

    struct TransactionResponse:
      public Response,
      public ::plasma::Transaction
    {};


    struct CreateTransactionResponse : Response
    {
      std::string created_transaction_id;
      int remaining_invitations;
    };

    struct UpdateTransactionResponse : Response
    {
      std::string updated_transaction_id;
    };

    struct NetworksResponse : Response
    {
      std::list<std::string> networks;
    };

    struct Network
    {
      std::string _id;
      std::string owner;
      std::string name;
      std::string model;
      std::string root_block;
      std::string root_address;
      std::string group_block;
      std::string group_address;
      std::string descriptor;
      std::list<std::string> users;
    };

    struct NetworkResponse : Network, Response
    {
    };

    struct CreateNetworkResponse : Response
    {
      std::string             created_network_id;
    };

    struct DeleteNetworkResponse : Response
    {
      std::string             deleted_network_id;
    };

    struct UpdateNetworkResponse : Response
    {
      std::string             updated_network_id;
    };

    struct NetworkNodesResponse : Response
    {
      std::string             network_id;
      std::list<std::string>       nodes;
    };

    struct ConnectDeviceResponse:
      public Response
    {};

    struct EndpointNodeResponse : Response
    {
      std::vector<std::string>      externals;
      std::vector<std::string>      locals;
      std::vector<std::string>      fallback;
    };

    typedef UpdateNetworkResponse NetworkAddUserResponse;
    typedef UpdateNetworkResponse NetworkAddDeviceResponse;
    typedef UpdateNetworkResponse NetworkConnectDeviceResponse;

    struct Device
    {
      std::string             id;
      std::string             name;
      std::string             passport;
    };

    struct CreateDeviceResponse : Response, Device
    {};

    struct UpdateDeviceResponse : Response, Device
    {};

    /// Callbacks for API calls.
    typedef std::function<void(LoginResponse const&)> LoginCallback;
    typedef std::function<void(RegisterResponse const&)> RegisterCallback;
    typedef std::function<void(NetworksResponse const&)> NetworksCallback;
    typedef std::function<void(NetworkResponse const&)> NetworkCallback;
    typedef std::function<void(CreateDeviceResponse const&)> CreateDeviceCallback;
    typedef std::function<void(UpdateDeviceResponse const&)> UpdateDeviceCallback;
    typedef std::function<void(UpdateNetworkResponse const&)> UpdateNetworkCallback;
    typedef std::function<void(NetworkNodesResponse const&)> NetworkNodesCallback;


    typedef elle::Buffer UserIcon;

    class Client: public elle::Printable
    {
      ELLE_ATTRIBUTE_R(std::string, host);
      ELLE_ATTRIBUTE_R(uint16_t, port);

    private:
      ELLE_ATTRIBUTE_R(std::string, root_url);
      //bool _check_errors;
      ELLE_ATTRIBUTE_RW(std::string, identity);
      ELLE_ATTRIBUTE_RW(std::string, email);
      ELLE_ATTRIBUTE_RW(std::string, token);
      ELLE_ATTRIBUTE_R(std::string, user_agent);

    public:
      Client(std::string const& server,
             uint16_t port,
             bool check_errors = true);
      ~Client();


    /*---------.
    | Requests |
    `---------*/
    public:
      template <typename T>
      T
      _post(std::string const& url,
            elle::format::json::Object const& req) const;
      void
      _post(std::string const& url,
            elle::format::json::Object const& req,
            std::ostream& resp) const;

      template <typename T>
      T
      _get(std::string const& url) const;

      void
      _get(std::string const& url,
           std::ostream& resp) const;

      template <typename T>
      T
      _deserialize_answer(std::istream& res) const;

    public:
      DebugResponse
      debug() const;

      LoginResponse
      login(std::string const& email,
            std::string const& password);

      LoginResponse
      generate_token(std::string const& token_genkey);

      LogoutResponse
      logout();

      RegisterResponse
      register_(std::string const& email,
                std::string const& fullname,
                std::string const& password,
                std::string const& activation_code,
                std::string const& picture_name = "",
                std::string const& picture_data = ""
      ) const;

      UserResponse
      user(std::string const& id) const;

      UserIcon
      user_icon(std::string const& id) const;

      SelfResponse
      self() const;

      UserResponse
      user_from_public_key(std::string const& public_key) const;

      UsersResponse
      search_users(std::string const& text, int count = 10, int offset = 0) const;

      SwaggersResponse
      get_swaggers() const;

      AddSwaggerResponse
      add_swaggers(std::string const& user1, std::string const& user2) const;

      Response
      ghostify(std::string const& email) const;

      Response
      favorite(std::string const& user) const;

      Response
      unfavorite(std::string const& user) const;

      Response
      genocide() const;

      // SwaggerResponse
      // get_swagger(std::string const& id) const;

      CreateDeviceResponse
      create_device(std::string const& name) const;

      UpdateDeviceResponse
      update_device(std::string const& _id,
                    std::string const& name) const;

      InviteUserResponse
      invite_user(std::string const& email) const;

      TransactionResponse
      transaction(std::string const& _id) const;

      /// Get the list of transactions that have a spectific status.
      /// The status can be pass threw 'status' list argument, allowing to
      /// search for many status matching.
      /// 'inclusive' is used to inverse the research result, by searching
      /// inclusivly or exclusivly in the status list.
      /// NB: If you let the default argument, it returns the 'non finished'
      /// transactions.
      /// 'count' is use to set a limit of transactions id to be pulled.
      TransactionsResponse
      transactions(std::vector<TransactionStatus> const& status = {},
                   bool inclusive = true,
                   int count = 0) const;

      CreateTransactionResponse
      create_transaction(std::string const& recipient_id_or_email,
                         std::list<std::string> const& files,
                         size_t count,
                         size_t size,
                         bool is_dir,
                         std::string const& device_id,
                         std::string const& message = "") const;

      UpdateTransactionResponse
      update_transaction(std::string const& transaction_id,
                         plasma::TransactionStatus status,
                         std::string const& device_id = "",
                         std::string const& device_name = "") const;

      UpdateTransactionResponse
      accept_transaction(std::string const& transaction_id,
                         std::string const& device_id,
                         std::string const& device_name) const;

      MessageResponse
      send_message(std::string const& recipient_id,
                   std::string const& sender_id, // DEBUG.
                   std::string const& message) const;

      PullNotificationResponse
      pull_notifications(int const count,
                         int const offset = 0) const;

      ReadNotificationResponse
      notification_read() const;

      NetworkResponse
      network(std::string const& _id) const;

      NetworksResponse
      networks() const;

      CreateNetworkResponse
      create_network(std::string const& network_id) const;

      DeleteNetworkResponse
      delete_network(std::string const& network_id,
                     bool force = false) const;

      NetworkNodesResponse
      network_nodes(std::string const& network_id) const;

      UpdateNetworkResponse
      update_network(std::string const& _id,
                     std::string const* name,
                     std::string const* root_block,
                     std::string const* root_address,
                     std::string const* group_block,
                     std::string const* group_address) const;

      NetworkAddUserResponse
      network_add_user(std::string const& network_id,
                       std::string const& user_id) const;

      NetworkAddDeviceResponse
      network_add_device(std::string const& network_id,
                         std::string const& device_id) const;

    private:

      typedef std::vector<std::pair<std::string, uint16_t>> adapter_type;


    public:
      ConnectDeviceResponse
      connect_device(std::string const& transaction_id,
                             std::string const& device_id,
                             adapter_type const& local_endpoints,
                             adapter_type const& public_endpoints) const;

    public:
      EndpointNodeResponse
      device_endpoints(std::string const& transaction_id,
                       std::string const& self_device_id,
                       std::string const& peer_device_id) const;

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const override;
    };

    std::ostream&
    operator <<(std::ostream& out,
                Error e);
  }
}

#include "Client.hxx"

#endif
