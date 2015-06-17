#ifndef PLASMA_META_CLIENT_HH
# define PLASMA_META_CLIENT_HH

# include <algorithm>
# include <functional>
# include <list>
# include <map>
# include <memory>
# include <stdexcept>
# include <string>
# include <vector>

# include <boost/random.hpp>
# include <boost/uuid/uuid.hpp>

# include <elle/Buffer.hh>
# include <elle/Exception.hh>
# include <elle/HttpClient.hh> // XXX: Remove that. Only for exception.
# include <elle/UUID.hh>
# include <elle/format/json/fwd.hh>
# include <elle/json/json.hh>
# include <elle/log.hh>
# include <elle/serialization/fwd.hh>
# include <elle/serialization/json.hh>
# include <elle/serialization/Serializer.hh>

# include <aws/Credentials.hh>

# include <reactor/http/Client.hh>
# include <reactor/http/Request.hh>

# include <infinit/oracles/LinkTransaction.hh>
# include <infinit/oracles/PeerTransaction.hh>
# include <infinit/oracles/meta/Account.hh>
# include <infinit/oracles/meta/AddressBookContact.hh>
# include <infinit/oracles/meta/Device.hh>
# include <infinit/oracles/meta/Error.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      std::string
      old_password_hash(std::string const& email,
                        std::string const& password);

      namespace json = elle::format::json;

      class Exception
        : public elle::Exception
      {
      public:
        Error const err;
        Exception(Error const& error, std::string const& message = "");

      public:
        bool operator ==(Exception const& e) const;
        bool operator ==(Error const& error) const;
        bool operator !=(Error const& error) const;
      };

      class QuotaExceeded
        : public elle::Error
      {
      public:
        QuotaExceeded(std::string const& reason,
                      uint64_t quota,
                      uint64_t usage)
          : elle::Error(reason)
          , _quota(quota)
          , _usage(usage)
        {}

        ELLE_ATTRIBUTE_R(uint64_t, quota);
        ELLE_ATTRIBUTE_R(uint64_t, usage);
      };

      struct User:
         public elle::Printable
      {
        typedef std::vector<elle::UUID> Devices;
        std::string id;
        std::string fullname;
        std::string handle;
        std::string register_status;
        Devices connected_devices;
        std::string public_key;
        boost::optional<std::string> ghost_code;
        boost::optional<std::string> ghost_profile_url;
        boost::optional<std::string> phone_number;
        User() = default;

        User(std::string const& id,
             std::string const& fullname,
             std::string const& handle,
             std::string const& register_status,
             Devices const& connected_devices = {},
             std::string const& public_key = "");

        bool
        online() const
        {
          return !this->connected_devices.empty();
        }

        bool
        online(elle::UUID const& device_id) const
        {
          return std::find(std::begin(this->connected_devices),
                           std::end(this->connected_devices),
                           device_id) != std::end(this->connected_devices);
        }

        // Check if there are devices other than that given online.
        // This is used when sending to self to determine if we need to cloud
        // buffer.
        // A simpler function could be used (such as len(devices) < 2) but
        // this would only make sense for ourself.
        bool
        online_excluding_device(elle::UUID const& device_id) const
        {
          bool res = false;
          for (auto const& device: this->connected_devices)
          {
            if (device != device_id)
            {
              res = true;
              break;
            }
          }
          return res;
        }

        bool
        ghost() const
        {
          if (register_status == "ghost")
            return true;
          else
            return false;
        }

        bool
        deleted() const
        {
          if (register_status == "deleted" || register_status == "merged")
            return true;
          else
            return false;
        }

        User(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);

        /*----------.
        | Printable |
        `----------*/
      public:
        virtual
        void
        print(std::ostream& stream) const override;

      };

      struct Self : User
      {
      public:
        Self() = default;
        Self(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
        std::string identity;
        boost::optional<std::string> email;
        boost::optional<std::string> facebook_id;
        int remaining_invitations;
        std::string token_generation_key;
        // std::list<boost::uuids::uuid> devices;
        std::list<std::string> devices;
        std::list<std::string> favorites;

        std::string
        identifier() const;
      };

      struct RegisterResponse
      {
        RegisterResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::SerializerIn& s);

        std::string id;
        boost::optional<std::string> ghost_code;
      };

      struct LoginResponse
      {
        Device device;
        Self self;
        struct Trophonius
        {
          std::string host;
          int port;
          int port_ssl;
          Trophonius() = default;
          Trophonius(elle::serialization::SerializerIn& s);
          void
          serialize(elle::serialization::Serializer& s);
        };
        Trophonius trophonius;
        std::unordered_map<std::string, std::string> features;
        bool account_registered; // True if this login was actually a registration.
        // XXX: Because facebook connect can turn a ghost to a real account a
        // ghost code can be consumed.
        boost::optional<std::string> ghost_code;
        LoginResponse() = default;
        LoginResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
      };

      struct SynchronizeResponse
      {
        // Self self;
        std::vector<User> swaggers;
        std::unordered_map<std::string, PeerTransaction> transactions;
        std::vector<LinkTransaction> links;
        std::vector<Device> devices;
        std::vector<Account> accounts;

        SynchronizeResponse() = default;
        SynchronizeResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
      };

      struct ServerStatus
      {
      public:
        ServerStatus() = default;
        ServerStatus(bool status, std::string message);
        ServerStatus(elle::serialization::SerializerIn& s);
        bool status;
        std::string message;
      protected:
        void
        serialize(elle::serialization::Serializer& s);
      };

      struct Fallback
      {
      public:
        Fallback() = default;
        Fallback(elle::serialization::SerializerIn& s);
        std::string fallback_host;
        int fallback_port_ssl;
        int fallback_port_tcp;
      protected:
        void
        serialize(elle::serialization::Serializer& s);
      };

      class CreatePeerTransactionResponse
      {
      public:
        CreatePeerTransactionResponse() = default;
        ELLE_ATTRIBUTE_R(User, recipient);
        ELLE_ATTRIBUTE_R(std::string, created_transaction_id);
        ELLE_ATTRIBUTE_R(bool, recipient_is_ghost);

        CreatePeerTransactionResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
      };

      class CloudCredentials: public elle::serialization::VirtuallySerializable
      {
      public:
        static constexpr char const* virtually_serializable_key = "protocol";
        virtual CloudCredentials* clone() const = 0;
      };

      class CloudCredentialsGCS: public CloudCredentials
      {
      public:
        CloudCredentialsGCS(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
        CloudCredentials*
        clone() const override;
        ELLE_ATTRIBUTE_R(std::string,  url);
        ELLE_ATTRIBUTE_R(boost::posix_time::ptime, server_time);
        ELLE_ATTRIBUTE_R(boost::posix_time::ptime, expiry);
      };

      class CloudCredentialsAws
        : public CloudCredentials
        , public aws::Credentials
      {
      public:
        CloudCredentialsAws(std::string const& access_key_id,
                            std::string const& secret_access_key,
                            std::string const& session_token,
                            std::string const& region,
                            std::string const& bucket,
                            std::string const& folder,
                            boost::posix_time::ptime expiration,
                            boost::posix_time::ptime server_time);
        CloudCredentialsAws(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
        CloudCredentials*
        clone() const override;
      };

      class UpdatePeerTransactionResponse
      {
      public:
        UpdatePeerTransactionResponse() = default;
        ELLE_ATTRIBUTE_R(std::unique_ptr<CloudCredentials>, cloud_credentials);
        // The ghost invitation code.
        ELLE_ATTRIBUTE_R(boost::optional<std::string>, ghost_code);
        // The ghost profile url.
        ELLE_ATTRIBUTE_R(boost::optional<std::string>, ghost_profile_url);
        ELLE_ATTRIBUTE_R(boost::optional<bool>, paused);
        UpdatePeerTransactionResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
      };

      class CreateLinkTransactionResponse
      {
      public:
        CreateLinkTransactionResponse() = default;
        CreateLinkTransactionResponse(CreateLinkTransactionResponse&&)
          = default;
        ELLE_ATTRIBUTE_R(LinkTransaction, transaction);
        ELLE_ATTRIBUTE_X(std::unique_ptr<CloudCredentials>, cloud_credentials);

        CreateLinkTransactionResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
      };

      class PlainInvitationResponse
      {
      public:
        PlainInvitationResponse() = default;

        ELLE_ATTRIBUTE_R(std::string, identifier);
        ELLE_ATTRIBUTE_R(std::string, ghost_code);
        ELLE_ATTRIBUTE_R(std::string, ghost_profile_url);

        PlainInvitationResponse(elle::serialization::SerializerIn& s);
        void
        serialize(elle::serialization::Serializer& s);
      };

      typedef elle::ConstWeakBuffer UserIcon;

      class Client: public elle::Printable
      {
        ELLE_ATTRIBUTE_R(std::string, protocol);
        ELLE_ATTRIBUTE_R(std::string, host);
        ELLE_ATTRIBUTE_R(uint16_t, port);

      private:
        ELLE_ATTRIBUTE_R(std::string, root_url);
        ELLE_ATTRIBUTE_P(reactor::http::Client, client, mutable);
        ELLE_ATTRIBUTE_X(reactor::http::Request::Configuration,
                          default_configuration);
        ELLE_ATTRIBUTE_RW(std::string, email);
        ELLE_ATTRIBUTE(std::string, session_id);
        typedef std::unordered_map<
          reactor::http::StatusCode,
          std::function<void (std::string const&)>> ErrorHandlers;
        ELLE_ATTRIBUTE_RX(ErrorHandlers, error_handlers);
      public:
        std::string
        session_id() const;

        void
        session_id(std::string const&);

      /*-------------.
      | Construction |
      `-------------*/
      public:
        Client(std::string const& meta);
        Client(std::string const& protocol,
               std::string const& server,
               uint16_t port);
        virtual
        ~Client();
        Client(const Client&) = delete;
        void operator = (const Client&) = delete;
      private:
        Client(std::tuple<std::string, std::string, int> meta);

      /*--------.
      | Helpers |
      `--------*/
      protected:
        typedef std::function<void (reactor::http::Request&)> Sender;
        reactor::http::Request
        _request(
          std::string const& url,
          reactor::http::Method method,
          bool throw_on_status = true) const;
        reactor::http::Request
        _request(
          std::string const& url,
          reactor::http::Method method,
          Sender const& send,
          bool throw_on_status = true) const;
        reactor::http::Request
        _request(
          std::string const& url,
          reactor::http::Method method,
          reactor::http::Request::QueryDict const& query_dict,
          boost::optional<Sender> const& send = boost::optional<Sender>(),
          boost::optional<std::string> const& content_type =
            boost::optional<std::string>(),
          bool throw_on_status = true) const;
        virtual
        void
        _pacify_retry(int& retry_count) const;
      private:
        std::string
        _url(std::string const& path) const;
        void
        _handle_errors(reactor::http::Request& request) const;
        boost::random::mt19937 mutable _rng;

        void
        _log_device(boost::optional<std::string> push_token,
                    boost::optional<std::string> country,
                    boost::optional<std::string> model,
                    boost::optional<std::string> name,
                    boost::optional<std::string> language);

      public:
        typedef std::pair<std::string, std::string> EmailPasswordPair;
        typedef std::string FacebookToken;
      private:
        typedef std::function<void (elle::serialization::json::SerializerOut&)>
          ParametersUpdater;
        LoginResponse
        _login(ParametersUpdater parameters_updater,
               boost::uuids::uuid const& device_uuid,
               boost::optional<std::string> device_push_token = boost::none,
               boost::optional<std::string> country_code = boost::none,
               boost::optional<std::string> device_model = boost::none,
               boost::optional<std::string> device_name = boost::none,
               boost::optional<std::string> device_language = boost::none);
      public:
        LoginResponse
        login(
          std::string const& email,
          std::string const& password,
          boost::uuids::uuid const& device_uuid,
          boost::optional<std::string> device_push_token = boost::none,
          boost::optional<std::string> country_code = boost::none,
          boost::optional<std::string> device_model = boost::none,
          boost::optional<std::string> device_name = boost::none,
          boost::optional<std::string> device_language = boost::none);

        LoginResponse
        facebook_connect(
          std::string const& facebok_token,
          boost::uuids::uuid const& device_uuid,
          boost::optional<std::string> preferred_email = boost::none,
          boost::optional<std::string> device_push_token = boost::none,
          boost::optional<std::string> country_code = boost::none,
          boost::optional<std::string> device_model = boost::none,
          boost::optional<std::string> device_name = boost::none,
          boost::optional<std::string> device_language = boost::none);

        bool
        facebook_id_already_registered(std::string const& facebook_id) const;

        /// Add Facebook account so that user can find Facebook friends.
        void
        add_facebook_account(std::string const& facebook_token) const;

        void
        logout();

        LoginResponse::Trophonius
        trophonius();

        SynchronizeResponse
        synchronize(bool init = false) const;

        ELLE_ATTRIBUTE_RW(bool, logged_in);

        RegisterResponse
        register_(std::string const& email,
                  std::string const& fullname,
                  std::string const& password) const;

        User
        user(std::string const& id) const;

        Self
        self() const;

        /// 'identifier' can be either email address or phone number.
        PlainInvitationResponse
        plain_invite_contact(std::string const& identifier) const;

        void
        send_invite(std::string const& destination,
                    std::string const& message,
                    std::string const& ghost_code) const;

        void
        use_ghost_code(std::string const& code) const;

        bool
        check_ghost_code(std::string const& code_) const;

        User
        user_from_handle(std::string const& handle) const;

        std::unordered_map<std::string, User>
        search_users_by_emails(std::vector<std::string> const& emails,
                               int limit = 10,
                               int offset = 0) const;

        std::vector<User>
        users_search(std::string const& text,
                     int limit = 5,
                     int offset = 0) const;

        std::vector<User>
        get_swaggers() const;

        void
        favorite(std::string const& user) const;

        void
        unfavorite(std::string const& user) const;

        // SwaggerResponse
        // get_swagger(std::string const& id) const;

        std::vector<Device>
        devices() const;

        Device
        update_device(elle::UUID const& device_uuid,
                      boost::optional<std::string> const& name = {},
                      boost::optional<std::string> const& model = {},
                      boost::optional<std::string> const& os = {}) const;

        Device
        device(boost::uuids::uuid const& device_id) const;

        void
        remove_swagger(std::string const& _id) const;

        PeerTransaction
        transaction(std::string const& _id) const;

        /// Fetch the list of transactions which have spectific statuses.
        /// A list of statuses to match can be passed through the 'statuses'
        /// list argument.
        /// 'negate' is used to inverse the search result, by either filtering
        /// for or by the 'statuses'.
        /// 'count' is use to set a limit to the number of transactions fetched.
        /// NB: The default arguments return the server's default response:
        /// all the unfinished transactions.
        std::vector<PeerTransaction>
        transactions(std::vector<Transaction::Status> const& statuses = {},
                     bool negate = false,
                     int count = 0) const;

        CreatePeerTransactionResponse
        create_transaction(
          std::string const& recipient_id_or_email,
          std::list<std::string> const& files,
          uint64_t count,
          uint64_t size,
          bool is_dir,
          elle::UUID const& device_uuid,
          std::string const& message = "",
          boost::optional<std::string const&> transaction_id = boost::none,
          boost::optional<elle::UUID> recipient_device_id = {}
          ) const;

        /// Create an empty transaction
        /// Deprecated by barebones overload
        /// @return: the transaction_id
        std::string
        create_transaction() const;

        UpdatePeerTransactionResponse
        update_transaction(std::string const& transaction_id,
                           boost::optional<Transaction::Status> status = boost::none,
                           elle::UUID const& device_id = elle::UUID(),
                           std::string const& device_name = "",
                           boost::optional<bool> pause = boost::none) const;

        std::string
        create_transaction(std::string const& recipient_id_or_email,
                           std::list<std::string> const& files,
                           uint64_t count,
                           std::string const& message = "") const;

      private:
        typedef std::vector<std::pair<std::string, uint16_t>> adapter_type;

      public:
        void
        transaction_endpoints_put(std::string const& transaction_id,
                                  elle::UUID const& device_id,
                                  adapter_type const& local_endpoints,
                                  adapter_type const& public_endpoints) const;

      public:
        Fallback
        fallback(std::string const& _id) const;

        std::unique_ptr<CloudCredentials>
        get_cloud_buffer_token(std::string const& transaction_id,
                               bool force_regenerate) const;

      /*------.
      | Links |
      `------*/
      public:
        CreateLinkTransactionResponse
        create_link(LinkTransaction::FileList const& file_list,
                    std::string const& name,
                    std::string const& message,
                    bool screenshot,
                    boost::optional<std::string const&> link_id = boost::none) const;

        /// create an empty link, to be initialized later.
        /// @return: the link ID.
        std::string
        create_link() const;



        void
        update_link(std::string const& id,
                    boost::optional<double> progress = boost::none,
                    boost::optional<Transaction::Status> status = boost::none,
                    boost::optional<bool> pause = boost::none) const;
        std::vector<LinkTransaction>
        links(int offset = 0,
              int count = 500,
              bool include_expired = false) const;
        std::unique_ptr<CloudCredentials>
        link_credentials(std::string const& id,
                         bool regenerate = false) const;

      public:
        ServerStatus
        server_status() const;

      /*----------.
      | Self User |
      `----------*/
      public:
        void
        change_email(std::string const& email,
                     std::string const& password) const;

        void
        change_password(std::string const& old_password,
                        std::string const& new_password) const;

        void
        edit_user(std::string const& fullname, std::string const& handle) const;

        elle::Buffer
        icon(std::string const& user_id) const;

        void
        icon(elle::ConstWeakBuffer const& icon) const;

        void
        upload_address_book(std::string const& json) const;

        void
        upload_address_book(std::vector<AddressBookContact> contacts) const;

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

# include <infinit/oracles/meta/Client.hxx>

#endif
