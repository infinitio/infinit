#ifndef SURFACE_GAP_STATE_HH
# define SURFACE_GAP_STATE_HH

# include <exception>
# include <map>
# include <mutex>
# include <string>
# include <unordered_set>

# include <boost/signals2.hpp>

# include <common/common.hh>

# include <elle/format/json/fwd.hh>
# include <elle/Printable.hh>
# include <elle/threading/Monitor.hh>
# include <elle/utility/Move.hh>

# include <reactor/mutex.hh>
# include <reactor/scheduler.hh>
# include <reactor/MultiLockBarrier.hh>
# include <reactor/thread.hh>
# include <reactor/network/proxy.hh>

# include <aws/S3.hh>

# include <papier/fwd.hh>

# include <infinit/metrics/CompositeReporter.hh>
# include <infinit/oracles/meta/Client.hh>
# include <infinit/oracles/Transaction.hh>
# include <infinit/oracles/trophonius/fwd.hh>

# include <surface/gap/Exception.hh>
# include <surface/gap/Model.hh>
# include <surface/gap/Notification.hh>
# include <surface/gap/PlainInvitation.hh>
# include <surface/gap/Self.hh>
# include <surface/gap/Transaction.hh>
# include <surface/gap/User.hh>
# include <surface/gap/gap.hh>

namespace surface
{
  namespace gap
  {
    static const uint32_t null_id = 0;

    // XXX: In order to ensure the logger is initialized at the begining of
    // state LoggerInitializer MUST be the first member of State.
    class LoggerInitializer
    {
    public:
      LoggerInitializer();
      ~LoggerInitializer();

    private:
      ELLE_ATTRIBUTE(std::unique_ptr<std::ofstream>, output);
    };

    class State:
      public elle::Printable
    {
    public:
      /*-------.
      | Logger |
      `-------*/
      // XXX: LoggerInitializer must be the first member of state.
      // It's construction forces the instantiation of the logger.
      LoggerInitializer _logger_intializer;

      /*--------.
      | Servers |
      `--------*/
      /// Metrics are sent using HTTP.
      ELLE_ATTRIBUTE_R(reactor::network::Proxy, http_proxy);
      /// Meta is HTTPS on production.
      ELLE_ATTRIBUTE_R(reactor::network::Proxy, https_proxy);
      /// Trophonius and Apertus will likely need SOCKS.
      ELLE_ATTRIBUTE_R(reactor::network::Proxy, socks_proxy);
      ELLE_ATTRIBUTE(infinit::oracles::meta::Client, meta);
      ELLE_ATTRIBUTE_R(std::string, meta_message);
      ELLE_ATTRIBUTE(std::vector<unsigned char>, trophonius_fingerprint);
      ELLE_ATTRIBUTE_RX(std::unique_ptr<infinit::oracles::trophonius::Client>,
                        trophonius);
      ELLE_ATTRIBUTE(std::string, forced_trophonius_host);
      ELLE_ATTRIBUTE(int, forced_trophonius_port);
      ELLE_ATTRIBUTE_RW(boost::optional<aws::URL>, s3_hostname);

    public:
      /// Meta session ID.
      std::string
      session_id() const;

    private:
      void
      _check_forced_trophonius();

      /*--------.
      | Network |
      `--------*/
    public:
      void
      internet_connection(bool connected);

      void
      set_proxy(reactor::network::Proxy const& proxy);

      void
      unset_proxy(reactor::network::ProxyType const& proxy_type);

    private:
      void
      _set_http_proxy(reactor::network::Proxy const& proxy);
      void
      _set_https_proxy(reactor::network::Proxy const& proxy);
      void
      _set_socks_proxy(reactor::network::Proxy const& proxy);

    public:
      infinit::oracles::meta::Client const&
      meta(bool authentication_required = true) const;

      bool
      logged_in_to_meta() const;
    private:
      bool
      _trophonius_server_check();

      ELLE_ATTRIBUTE_R(
        std::unique_ptr<infinit::oracles::meta::SynchronizeResponse>,
        synchronize_response);
    /*--------.
    | Metrics |
    `--------*/
    private:
      ELLE_ATTRIBUTE_RP(std::unique_ptr<infinit::metrics::Reporter>,
                        metrics_reporter,
                        mutable);

      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>,
                     metrics_heartbeat_thread);

      /*-------------.
      | Construction |
      `-------------*/
    public:
      class ConnectionStatus:
        public Notification
      {
      public:
        static Notification::Type type;
        ConnectionStatus(bool status,
                         bool still_trying,
                         std::string const& last_error);

        bool status;       ///< Connection status
        bool still_trying; ///< Is the State still trying to connect
        std::string last_error;
      };

    public:
      State(common::infinit::Configuration local_config);
      State(std::string const& meta_protocol,
            std::string const& meta_host,
            uint16_t meta_port,
            std::vector<unsigned char> trophonius_fingerprint,
            boost::optional<boost::uuids::uuid const&> device_id = {},
            boost::optional<std::string> download_dir = {},
            boost::optional<std::string> home_dir = {},
            boost::optional<papier::Authority> authority = {});
      ~State();
      ELLE_ATTRIBUTE_R(boost::filesystem::path, home);

      ELLE_ATTRIBUTE_RP(
        common::infinit::Configuration, local_configuration, mutable);

    private:
      void
      _check_first_launch();

    public:
      //- Login & register -----------------------------------------------------
      std::unique_ptr<Self> mutable _me;

      Self const&
      me() const;

      void
      update_me();

      void
      set_avatar(boost::filesystem::path const& image_path);

      void
      set_avatar(elle::Buffer const& avatar);

      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, login_thread);
      ELLE_ATTRIBUTE(reactor::Mutex, login_mutex);
      ELLE_ATTRIBUTE_X(reactor::Barrier, logged_in);
      ELLE_ATTRIBUTE_X(reactor::Barrier, logged_out);
    public:
      typedef std::unique_ptr<infinit::oracles::trophonius::Client>
      TrophoniusClientPtr;

      /// Used to allow users to login to the website automatically.
      std::string
      web_login_token();

      /// Keep trying, returns on success or throw on definitive failure.
      void
      login(std::string const& email,
            std::string const& password,
            boost::optional<std::string> device_push_token = boost::none,
            boost::optional<std::string> country_code = boost::none,
            boost::optional<std::string> device_model = boost::none,
            boost::optional<std::string> device_name = boost::none,
            boost::optional<std::string> device_language = boost::none);

      /// Keep trying, returns on success or throw on definitive failure.
      void
      login(std::string const& email,
            std::string const& password,
            reactor::DurationOpt timeout,
            boost::optional<std::string> country_code = boost::none,
            boost::optional<std::string> device_model = boost::none,
            boost::optional<std::string> device_name = boost::none,
            boost::optional<std::string> device_language = boost::none);

      void
      login(std::string const& email,
            std::string const& password,
            reactor::DurationOpt timeout,
            boost::optional<std::string> device_push_token,
            boost::optional<std::string> country_code,
            boost::optional<std::string> device_model,
            boost::optional<std::string> device_name,
            boost::optional<std::string> device_language);

      /// Login to meta.
      void
      login(
        std::string const& email,
        std::string const& password,
        TrophoniusClientPtr trophonius,
        reactor::DurationOpt timeout = reactor::DurationOpt(),
        boost::optional<std::string> device_push_token = boost::none,
        boost::optional<std::string> country_code = boost::none,
        boost::optional<std::string> device_model = boost::none,
        boost::optional<std::string> device_name = boost::none,
        boost::optional<std::string> device_language = boost::none);

      /// Logout from meta.
      void
      logout();

      /// disconnect from tropho.
      void
      disconnect();

      /// Reconnect to trophonius, assuming we are still logged in.
      void
      connect();

      void
      register_(std::string const& fullname,
                std::string const& email,
                std::string const& password,
                boost::optional<std::string> device_push_token = boost::none,
                boost::optional<std::string> country_code = boost::none,
                boost::optional<std::string> device_model = boost::none,
                boost::optional<std::string> device_name = boost::none,
                boost::optional<std::string> device_language = boost::none);

      /// Login / Register the user using facebook.
      /// The code is generated by the front end using a facebook connect
      /// mechanism.
      void
      facebook_connect(
        std::string const& facebook_token,
        boost::optional<std::string> preferred_email = boost::none,
        boost::optional<std::string> device_push_token = boost::none,
        boost::optional<std::string> country_code = boost::none,
        boost::optional<std::string> device_model = boost::none,
        boost::optional<std::string> device_name = boost::none,
        boost::optional<std::string> device_language = boost::none);

      void
      facebook_connect(
        std::string const& facebook_token,
        TrophoniusClientPtr trophonius,
        boost::optional<std::string> preferred_email = boost::none,
        boost::optional<std::string> device_push_token = boost::none,
        boost::optional<std::string> country_code = boost::none,
        boost::optional<std::string> device_model = boost::none,
        boost::optional<std::string> device_name = boost::none,
        boost::optional<std::string> device_language = boost::none,
        reactor::DurationOpt timeout = reactor::DurationOpt());

      /// Add Facebook account so that user can find Facebook friends.
      void
      add_facebook_account(std::string const& facebook_token);

      std::string
      user_directory();

      ELLE_ATTRIBUTE_R(std::string, output_dir);
      /// Set the output directory.
      /// Fallback is true when it was not a user action. i.e.: existing path
      /// is not suitable any more and was changed by the application.
      void
      set_output_dir(std::string const& dir, bool fallback);

      /// Ensure that State's model is cleared and that there are no pending
      /// notifications for the UI.
      /// If the user is logged in, this will log the user out.
      void
      clean();

    private:
      ELLE_ATTRIBUTE_RW(boost::posix_time::time_duration,
        reconnection_cooldown);
      void
      _login(std::string const& email,
             std::string const& password,
             elle::utility::Move<TrophoniusClientPtr> trophonius,
             boost::optional<std::string> device_push_token = boost::none,
             boost::optional<std::string> country_code = boost::none,
             boost::optional<std::string> device_model = boost::none,
             boost::optional<std::string> device_name = boost::none,
             boost::optional<std::string> device_language = boost::none);

      // This function wrap all the retry mechanism.
      typedef std::function<
        void (bool success, std::string const& failure_reason)> LoginMetric;
      bool
      _login(
        std::function <infinit::oracles::meta::LoginResponse ()> login_function,
        elle::utility::Move<TrophoniusClientPtr> trophonius,
        std::function<std::string ()> identity_password,
        LoginMetric metric);

      // Wrap the login function with a timeout.
      void
      _login_with_timeout(std::function<void ()> login_function,
                          reactor::DurationOpt timeout);

      void
      _on_invalid_trophonius_credentials();
      void
      _cleanup();

    private:
      void
      _kick_out();

      void
      _kick_out(bool retry,
               std::string const& message);

      /*-------------------.
      | External Callbacks |
      `-------------------*/

      class _Runner
      {
      public:
        virtual
        ~_Runner() {};
        virtual
        void
        operator () () const = 0;
      };

      template <typename T>
      class Runner:
        public _Runner
      {
      public:
        typedef std::function<void (T const&)> Callback;

        Runner(Callback cb, T notif);
        ~Runner() {};

        void
        operator () () const override;

        Callback _cb;
        T _notification;
      };

      typedef std::function<void (Notification const&)> Callback;
      //ELLE_ATTRIBUTE_R(std::map<Notification::Type, std::vector<Callback>>, callbacks);
      mutable std::map<Notification::Type, std::vector<Callback>> _callbacks;
      // The unique_ptr is mandatory.
      //ELLE_ATTRIBUTE(std::queue<std::unique_ptr<_Runner>>, runners);
      mutable std::queue<std::unique_ptr<_Runner>> _runners;
    public:
      template <typename T>
      void
      attach_callback(std::function<void (T const&)> cb) const;

      template <typename T>
      void
      enqueue(T const& notif) const;

      void
      poll() const;

      ELLE_ATTRIBUTE_P(std::mutex, poll_lock, mutable);

      /*--------------.
      | Notifications |
      `--------------*/
      ELLE_ATTRIBUTE_R(std::unique_ptr<reactor::Thread>, polling_thread);

    public:
      void
      synchronize();

    public:
      void
      on_connection_changed(infinit::oracles::trophonius::ConnectionState const&,
                            bool first_connection=false);

      void
      on_reconnection_failed();

      void
      handle_notification(
        std::unique_ptr<infinit::oracles::trophonius::Notification>&& notification);

      // For lisibility purpose, papiers, user, network and transaction methods
      // are located in specific files in _detail.
      /*--------.
      | Papiers |
      `--------*/
    public:
      /// Get the remote device informations.
      Device const&
      device() const;

      void
      set_device_id(std::string const& device_id);

    private:
      ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_uuid);
      ELLE_ATTRIBUTE_P(std::unique_ptr<Device>, device, mutable);
      ELLE_ATTRIBUTE_P(std::unique_ptr<papier::Passport>, passport, mutable);
      ELLE_ATTRIBUTE_P(std::unique_ptr<papier::Identity>, identity, mutable);
    public:
      /// Get the local passport of the logged user.
      papier::Passport const&
      passport() const;

      /// Get the local identity of the logged user.
      papier::Identity const&
      identity() const;

    public:
      /// Check if the local device has been created.
      bool
      has_device() const;

      /// Update the local device name, model and OS.
      void
      update_device(boost::optional<std::string> name = {},
                    boost::optional<std::string> model = {},
                    boost::optional<std::string> os = {}) const;

      void
      change_password(std::string const& old_password,
                      std::string const& new_password);

      /*--------.
      | Account |
      `--------*/
    public:
      Account
      account() const;

      void
      performed_social_post(std::string const& medium); // facebook/twitter

    private:
      void
      _account(Account const& account);

      /*------------------.
      | External Accounts |
      `------------------*/
    public:
      std::vector<ExternalAccount const*>
      external_accounts() const;

    private:
      void
      _external_accounts(std::vector<ExternalAccount> const& accounts);

      /*--------.
      | Devices |
      `--------*/
    public:
      std::vector<gap::Device const*>
      devices() const;

      void
      _on_device_added(Device const& device) const;

      void
      _on_device_removed(elle::UUID id);

      void
      _on_devices_reseted();

      void
      _devices(std::vector<Device> const& devices);

      // Could be factorized.
      /*------.
      | Users |
      `------*/
      class UserNotFoundException:
        public Exception
      {
      public:
        UserNotFoundException(uint32_t id);
        UserNotFoundException(std::string const& id);
      };

      class NotASwaggerException:
        public Exception
      {
      public:
        NotASwaggerException(uint32_t id);
        NotASwaggerException(std::string const& id);
      };

      class UserStatusNotification:
        public Notification
      {
      public:
        static Notification::Type type;

        UserStatusNotification(uint32_t id,
                               bool status);

        uint32_t id;
        bool status;
      };

      class NewSwaggerNotification:
        public Notification
      {
      public:
        static Notification::Type type;

        NewSwaggerNotification(surface::gap::User const& user);

        surface::gap::User user;
      };

      class DeletedSwaggerNotification:
        public Notification
      {
      public:
        static Notification::Type type;
        DeletedSwaggerNotification(uint32_t id);
        uint32_t id;
      };

      class DeletedFavoriteNotification:
        public Notification
      {
      public:
        static Notification::Type type;
        DeletedFavoriteNotification(uint32_t id);
        uint32_t id;
      };

      class AvatarAvailableNotification:
        public Notification
      {
      public:
        static Notification::Type type;

        AvatarAvailableNotification(uint32_t id);

        uint32_t id;
      };

    public:

      typedef infinit::oracles::meta::User User;
      typedef std::pair<const uint32_t, User> UserPair;
      typedef std::unordered_map<uint32_t, User> UserMap;
      typedef std::unordered_set<User> Users;
      typedef std::map<std::string, uint32_t> UserIndexMap;
      typedef std::unordered_set<uint32_t> UserIndexes;
      typedef std::unordered_map<uint32_t, elle::Buffer> UserAvatars;
      typedef std::unordered_set<std::string> AvatarToFetch;

      ELLE_ATTRIBUTE_RP(UserMap, users, mutable);
      ELLE_ATTRIBUTE_RP(UserIndexMap, user_indexes, mutable);
      ELLE_ATTRIBUTE_RP(UserIndexes, swagger_indexes, mutable);
      ELLE_ATTRIBUTE_P(reactor::Mutex, swagger_mutex, mutable);
      ELLE_ATTRIBUTE_RP(UserAvatars, avatars, mutable);
      ELLE_ATTRIBUTE_RP(AvatarToFetch, avatar_to_fetch, mutable);

      ELLE_ATTRIBUTE_P(std::unique_ptr<reactor::Thread>, avatar_fetcher_thread,
                       mutable);
      ELLE_ATTRIBUTE_P(reactor::Barrier, avatar_fetching_barrier, mutable);

    public:

      /// Force the update of the model.
      User const&
      user_sync(User const& user,
                bool send_notification = true,
                bool is_swagger = false) const;

      /// Fetch the user from meta and update its model.
      User const&
      user_sync(std::string const& id,
                bool send_notification = true,
                bool is_swagger = false) const;

      /// Get a user by user_id. If it's not in the model and merge is set to
      /// true, merge it from meta, otherwise throw.
      User const&
      user(std::string const& user_id,
           bool merge = true) const;

      User const&
      user(uint32_t id) const;

      User const&
      user(std::function<bool (UserPair const&)> const& func) const;

      User const&
      user_from_handle(std::string const& handle) const;

      uint32_t
      user_id(std::string const& user_meta_id) const;

      void
      _user_resync(std::vector<User> const& users, bool login = false);

      void
      _queue_user_icon(std::string const& user_id) const;

      elle::ConstWeakBuffer
      user_icon(std::string const& user_id) const;

      void
      user_icon_refresh(uint32_t user_id) const;

      std::vector<surface::gap::User>
      users_search(std::string const& text) const;

      std::unordered_map<std::string, surface::gap::User>
      users_by_emails(std::vector<std::string> const& emails) const;

      bool
      device_status(std::string const& user_id,
                    elle::UUID const& device_id) const;

    public:
      surface::gap::User
      user_to_gap_user(uint32_t id,
                        State::User const& user) const;

    /*---------.
    | Swaggers |
    `---------*/
    public:
      UserIndexes
      swaggers();
      bool
      is_swagger(uint32_t id) const;
      User
      swagger(std::string const& user_id);
      User
      swagger(uint32_t id);
      void
      _on_new_swagger(
        infinit::oracles::trophonius::NewSwaggerNotification const& notif);
      void
      _on_deleted_swagger(
        infinit::oracles::trophonius::DeletedSwaggerNotification const& notif);
      void
      _on_deleted_favorite(
        infinit::oracles::trophonius::DeletedFavoriteNotification const& notif);
      void
      _on_swagger_status_update(
        infinit::oracles::trophonius::UserStatusNotification const& notif);
      void
      _on_swagger_status_update(std::string const& user_id,
                                bool user_status,
                                elle::UUID const& device_id,
                                bool device_status);
      ELLE_ATTRIBUTE_RX(
        (boost::signals2::signal<void (uint32_t user_id,
                                       std::string const& contact)>),
        contact_joined);

      /*-------------.
      | Transactions |
      `-------------*/
      class TransactionNotFoundException:
        public Exception
      {
      public:
        TransactionNotFoundException(uint32_t id);
        TransactionNotFoundException(std::string const& id);
      };

      typedef std::unique_ptr<Transaction> TransactionPtr;
      typedef std::pair<uint32_t, TransactionPtr> TransactionPair;
      typedef std::pair<const uint32_t, TransactionPtr> TransactionConstPair;
      typedef std::unordered_map<uint32_t, TransactionPtr> Transactions;
      typedef std::map<std::string, uint32_t> TransactionIndexMap;
      typedef std::unordered_set<uint32_t> TransactionIndexes;

      ELLE_ATTRIBUTE_R(Transactions, transactions);
      Transactions& transactions() {  return this->_transactions; }

      void
      transaction_pause(uint32_t id,
                        bool paused = true);

      /*------------------.
      | Link Transactions |
      `------------------*/

      uint32_t
      create_link(std::vector<std::string> const& files,
                  std::string const& message,
                  bool screenshot);

      surface::gap::LinkTransaction
      link_to_gap_link(
        uint32_t id,
        infinit::oracles::LinkTransaction const& transaction,
        gap_TransactionStatus status,
        boost::optional<gap_Status> status_info = boost::none) const;

      surface::gap::PeerTransaction
      transaction_to_gap_transaction(
        uint32_t id,
        infinit::oracles::PeerTransaction const& transaction,
        gap_TransactionStatus status,
        boost::optional<gap_Status> status_info = boost::none) const;
      /*------------------.
      | Peer Transactions |
      `------------------*/

      // Create peer transaction.
      Transaction&
      transaction_peer_create(std::string const& peer_id,
                              std::vector<std::string> files,
                              std::string const& message);
      // Create peer transaction to a specific device.
      Transaction&
      transaction_peer_create(std::string const& peer_id,
                              elle::UUID const& peer_device_id,
                              std::vector<std::string> files,
                              std::string const& message);
      /// Deprecated, see transaction_peer_create.
      uint32_t
      send_files(std::string const& peer_id,
                 std::vector<std::string> files,
                 std::string const& message);

      uint32_t
      start_onboarding(std::string const& file_path,
                       reactor::Duration const& transfer_duration = 5_sec);

      /// Catches user unknown exceptions and sets return to null.
      uint32_t
      user_id_or_null(std::string const& id) const;

    private:
      template <typename ... T>
      Transaction&
      _transaction_peer_create(std::string const& peer_id, T&& ... args);

      void
      _transactions_init();

      void
      _peer_transaction_resync(
        std::unordered_map<std::string, infinit::oracles::PeerTransaction> const& transactions,
        bool login = false);

      void
      _link_transaction_resync(
        std::vector<infinit::oracles::LinkTransaction> const& links,
        bool login = false);

      void
      _transactions_clear();

      void
      _on_transaction_update(
        std::shared_ptr<infinit::oracles::Transaction> const& notif);

      void
      _on_peer_reachability_updated(
        infinit::oracles::trophonius::PeerReachabilityNotification const& notif);

      void
      _on_transaction_paused(
        infinit::oracles::trophonius::PausedNotification const& notif);

    public:
      void
      on_transaction_paused(std::string const& transaction_id,
                            bool paused);

      mutable reactor::MultiLockBarrier transaction_update_lock;

    /*------------.
    | Invitations |
    `------------*/
    public:
      surface::gap::PlainInvitation
      plain_invite_contact(std::string const& identifier);

      void
      send_invite(std::string const& destination,
                  std::string const& message,
                  std::string const& ghost_code,
                  std::string const& invite_type,
                  bool user_cancel);

    /*------------.
    | Ghost codes |
    `------------*/
    public:
      void
      ghost_code_use(std::string const& code, bool was_link);
      struct GhostCode
      {
        std::string code;
        bool was_link;
      };
      ELLE_ATTRIBUTE_RX(
        (boost::signals2::signal<void (std::string const& code,
                                       bool success,
                                       std::string const& reason)>),
        ghost_code_used);

    private:
      void
      _ghost_code_snapshot();
      void
      _ghost_code_use();
      ELLE_ATTRIBUTE(std::vector<GhostCode>, ghost_codes);
      ELLE_ATTRIBUTE(std::string, referral_code);

    /*------------.
    | Fingerprint |
    `------------*/
    public:
      void
      fingerprint_add(std::string const& fingerprint);

    /*--------------.
    | Configuration |
    `--------------*/
    public:
      struct Configuration
      {
        // This is actualy used for both S3 and GCS. Rename.
        struct S3
        {
          // This is actually also used for peer buffering. Rename.
          struct MultipartUpload
          {
            int chunk_size;
            int parallelism;
            void serialize(elle::serialization::Serializer& s);
          };
          MultipartUpload multipart_upload;
          void serialize(elle::serialization::Serializer& s);
        };
        S3 s3;
        void serialize(elle::serialization::Serializer& s);
        bool enable_file_mirroring; // Only copy files if this is true.
        int64_t max_mirror_size; // Copy files to send if below this size
        int64_t max_compress_size; // Only compresss archive if content below this size
        int64_t max_cloud_buffer_size; // Only cloud buffer below this size
        bool disable_upnp;
        typedef std::unordered_map<std::string, std::string> Features;
        Features features;
      };
      ELLE_ATTRIBUTE_RP(Configuration, configuration, protected:);
      ELLE_ATTRIBUTE(std::string, email);
      ELLE_ATTRIBUTE(std::string, password);
      ELLE_ATTRIBUTE(reactor::Thread*, login_watcher_thread);
    private:
      void
      _apply_configuration(elle::json::Object json);

    /*------.
    | Model |
    `------*/
    public:
      ELLE_ATTRIBUTE_RX(Model, model);

    /*--------.
    | Message |
    `--------*/
    public:
      ELLE_ATTRIBUTE_RX(
        (boost::signals2::signal<void (std::string const& message)>),
        message_received);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;

      /*----------.
      | Debugging |
      `----------*/
      ELLE_ATTRIBUTE_RX(reactor::Signal, synchronized);

      ELLE_ATTRIBUTE_R(papier::Authority, authority);
    };

    std::ostream&
    operator <<(std::ostream& out,
                NotificationType const& t);
    std::ostream&
    operator <<(std::ostream& out,
                State::ConnectionStatus const& s);
  }
}

std::ostream&
operator <<(std::ostream& out,
            gap_TransactionStatus status);

#include <surface/gap/State.hxx>

#endif
