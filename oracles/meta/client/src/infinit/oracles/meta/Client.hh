#ifndef  PLASMA_META_CLIENT_HH
# define PLASMA_META_CLIENT_HH

# include <infinit/oracles/meta/Transaction.hh>

# include <elle/Exception.hh>
# include <elle/Buffer.hh>
# include <elle/HttpClient.hh>
# include <elle/format/json/fwd.hh>
# include <elle/log.hh>
# include <reactor/http/Request.hh>
# include <reactor/http/Client.hh>

#include <boost/uuid/uuid.hpp>

# include <functional>
# include <list>
# include <map>
# include <memory>
# include <stdexcept>
# include <string>
# include <vector>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      namespace json = elle::format::json;

      enum class Error: int
      {
# define ERR_CODE(name, value, comment)                                         \
        name = value,
#  include <infinit/oracles/meta/error_code.hh.inc>
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
        std::string device_id;
      };

      struct LogoutResponse : Response
      {};

      struct RegisterResponse:
        public Response
      {
        std::string registered_user_id;
        std::string invitation_source;
      };

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
      {
      };

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

      // struct InvitedResponse: Response
      // {
      //   std::vector<std::map<std::string, std::string>> emails;
      // };

      struct UsersResponse : Response
      {
        std::list<std::string> users;
      };

      struct RemoveSwaggerResponse: Response
      {};

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
        public Transaction
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

      struct ConnectDeviceResponse:
        public Response
      {};

      struct EndpointNodeResponse : Response
      {
        std::vector<std::string>      externals;
        std::vector<std::string>      locals;
        std::vector<std::string>      fallback;
      };

      struct Device
      {
        std::string id;
        std::string name;
        std::string passport;
      };

      struct CreateDeviceResponse : Response, Device
      {};

      struct DeviceResponse : Response, Device
      {};

      struct UpdateDeviceResponse : Response, Device
      {};

      struct DeleteDeviceResponse : Response
      {
        std::string id;
      };

      typedef elle::Buffer UserIcon;

      class Client: public elle::Printable
      {
        ELLE_ATTRIBUTE_R(std::string, host);
        ELLE_ATTRIBUTE_R(uint16_t, port);

      private:
        ELLE_ATTRIBUTE_R(std::string, root_url);
        ELLE_ATTRIBUTE_R(reactor::http::Client, client);
        ELLE_ATTRIBUTE_R(reactor::http::Request::Configuration,
                         default_configuration);
        ELLE_ATTRIBUTE_RW(std::string, email);
        ELLE_ATTRIBUTE_R(std::string, user_agent);

      public:
        Client(std::string const& server,
               uint16_t port);
        ~Client();


        /*---------.
        | Requests |
        `---------*/
      protected:
        template <typename T>
        T
        _request(std::string const& url,
                 reactor::http::Method method,
                 elle::format::json::Object const& req = {}) const;

        template <typename T>
        T
        _post(std::string const& url,
                      elle::format::json::Object const& req = {}) const;

        template <typename T>
        T
        _get(std::string const& url,
             elle::format::json::Object const& req = {}) const;

        template <typename T>
        T
        _put(std::string const& url,
             elle::format::json::Object const& req = {}) const;

        template <typename T>
        T
        _delete(std::string const& url,
                elle::format::json::Object const& req = {}) const;

        template <typename T>
        T
        _deserialize_answer(elle::Buffer& res) const;

      public:
        DebugResponse
        status() const;

        DebugResponse
        debug() const;

        LoginResponse
        login(std::string const& email,
              std::string const& password,
              boost::uuids::uuid const& device_uuid);

        LogoutResponse
        logout();

        RegisterResponse
        register_(std::string const& email,
                  std::string const& fullname,
                  std::string const& password,
                  std::string const& activation_code) const;

        UserResponse
        user(std::string const& id) const;

        SelfResponse
        self() const;

        UserResponse
        user_from_public_key(std::string const& public_key) const;

        UsersResponse
        search_users(std::string const& text, int count = 10, int offset = 0) const;

        SwaggersResponse
        get_swaggers() const;

        Response
        favorite(std::string const& user) const;

        Response
        unfavorite(std::string const& user) const;

        // SwaggerResponse
        // get_swagger(std::string const& id) const;

        CreateDeviceResponse
        create_device(std::string const& name) const;

        UpdateDeviceResponse
        update_device(std::string const& _id,
                      std::string const& name) const;

        DeviceResponse
        device(std::string const& _id) const;

        DeleteDeviceResponse
        delete_device(std::string const& _id) const;

        InviteUserResponse
        invite_user(std::string const& email) const;

        RemoveSwaggerResponse
        remove_swagger(std::string const& _id) const;

        // InvitedResponse
        // invited() const;

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
        transactions(std::vector<Transaction::Status> const& status = {},
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
                           Transaction::Status status,
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
}

#include "Client.hxx"

#endif
