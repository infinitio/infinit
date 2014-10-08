#ifndef SURFACE_GAP_STATE_HH
# define SURFACE_GAP_STATE_HH

# include <exception>
# include <map>
# include <mutex>
# include <string>
# include <unordered_set>

# include <elle/format/json/fwd.hh>
# include <elle/Printable.hh>
# include <elle/threading/Monitor.hh>

# include <reactor/mutex.hh>
# include <reactor/scheduler.hh>
# include <reactor/MultiLockBarrier.hh>
# include <reactor/thread.hh>
# include <reactor/network/proxy.hh>

# include <papier/fwd.hh>

# include <infinit/metrics/CompositeReporter.hh>
# include <infinit/oracles/meta/Client.hh>
# include <infinit/oracles/Transaction.hh>
# include <infinit/oracles/trophonius/fwd.hh>

# include <surface/gap/Device.hh>
# include <surface/gap/Exception.hh>
# include <surface/gap/gap.hh>
# include <surface/gap/Notification.hh>
# include <surface/gap/Self.hh>
# include <surface/gap/Transaction.hh>

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

    private:
      bool
      _meta_server_check(reactor::Duration);

      bool
      _meta_server_check();

      bool
      _trophonius_server_check();

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
        ConnectionStatus(bool status);

        bool status;
      };

      class KickedOut:
        public Notification
      {
      public:
        static Notification::Type type;
        KickedOut() = default;
      };

      class TrophoniusUnavailable:
        public Notification
      {
      public:
        static Notification::Type type;
        TrophoniusUnavailable() = default;
      };

    public:
      State(std::string const& meta_protocol,
            std::string const& meta_host,
            uint16_t meta_port,
            boost::uuids::uuid device,
            std::vector<unsigned char> trophonius_fingerprint,
            std::string const& download_dir,
            std::unique_ptr<infinit::metrics::Reporter> metrics = nullptr);
      ~State();

    private:
      void
      _check_first_launch();

    public:
      bool
      logged_in() const
      {
        return this->_meta.logged_in();
      }

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

      ELLE_ATTRIBUTE(reactor::Mutex, login_mutex);
    public:
      /// Login to meta.
      void
      login(std::string const& email, std::string const& password);
      /// Login to meta.
      void
      login(
        std::string const& email,
        std::string const& password,
        std::unique_ptr<infinit::oracles::trophonius::Client> trophonius);

      /// Logout from meta.
      void
      logout();

      void
      register_(std::string const& fullname,
                std::string const& email,
                std::string const& password);

      ///
      static
      std::string
      hash_password(std::string const& email,
                    std::string const& password);

      std::string
      user_directory();

      ELLE_ATTRIBUTE_R(std::string, output_dir);
      /// Set the output directory.
      /// Fallback is true when it was not a user action. i.e.: existing path
      /// is not suitable any more and was changed by the application.
      void
      set_output_dir(std::string const& dir, bool fallback);

    private:
      void
      _on_invalid_trophonius_credentials();
      void
      _cleanup();

      /*-------------------.
      | External Callbacks |
      `-------------------*/

      class _Runner
      {
      public:
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

      void
      on_connection_changed(bool connection_status, bool first_connection=false);

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
    private:
      ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_uuid);
      ELLE_ATTRIBUTE_P(std::unique_ptr<Device>, device, mutable);
      ELLE_ATTRIBUTE_P(reactor::Mutex, device_mutex, mutable);
    public:
      /// Get the remote device informations.
      Device const&
      device() const;

      ELLE_ATTRIBUTE_P(std::unique_ptr<papier::Passport>, passport, mutable);
      ELLE_ATTRIBUTE_P(reactor::Mutex, passport_mutex, mutable);
      ELLE_ATTRIBUTE_P(std::unique_ptr<papier::Identity>, identity, mutable);
      ELLE_ATTRIBUTE_P(reactor::Mutex, identity_mutex, mutable);
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

      /// Update the local device name.
      void
      update_device(std::string const& name) const;

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

        NewSwaggerNotification(uint32_t id);

        uint32_t id;
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
      ELLE_ATTRIBUTE(reactor::Mutex, swagger_mutex);
      ELLE_ATTRIBUTE_RP(UserAvatars, avatars, mutable);
      ELLE_ATTRIBUTE_RP(AvatarToFetch, avatar_to_fetch, mutable);

      ELLE_ATTRIBUTE_P(std::unique_ptr<reactor::Thread>, avatar_fetcher_thread,
                       mutable);
      ELLE_ATTRIBUTE_P(reactor::Barrier, avatar_fetching_barrier, mutable);

    public:
      void
      clear_users();

      User const&
      user_sync(User const& user) const;

      User const&
      user_sync(std::string const& id) const;

      User const&
      user(std::string const& user_id,
           bool merge = true) const;

      User const&
      user(uint32_t id) const;

      User const&
      user(std::function<bool (UserPair const&)> const& func) const;

      User const&
      user_from_handle(std::string const& handle) const;

      User const&
      user_from_public_key(std::string const& public_key) const;

      void
      _users_init();

      void
      _user_resync();

      void
      _queue_user_icon(std::string const& user_id) const;

      elle::ConstWeakBuffer
      user_icon(std::string const& user_id) const;

      void
      user_icon_refresh(uint32_t user_id) const;

      std::vector<uint32_t>
      users_search(std::string const& text) const;

      std::unordered_map<std::string, uint32_t>
      users_by_emails(std::vector<std::string> const& emails) const;

      bool
      device_status(std::string const& user_id,
                    std::string const& device_id) const;

      ///- Swaggers --------------------------------------------------------------
      UserIndexes
      swaggers();

      User
      swagger(std::string const& user_id);

      User
      swagger(uint32_t id);

      void
      swaggers_dirty();

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

      /*---------.
      | Networks |
      `---------*/
    //   class NetworkNotFoundException:
    //     public Exception
    //   {
    //   public:
    //     NetworkNotFoundException(uint32_t id);
    //     NetworkNotFoundException(std::string const& id);
    //   };

    //   typedef plasma::meta::Network Network;
    //   typedef std::pair<uint32_t, Network> NetworkPair;
    //   typedef std::unordered_map<uint32_t, Network> NetworkMap;
    //   typedef std::unordered_map<uint32_t, std::string> NetworkIndexMap;
    //   typedef std::unordered_set<uint32_t> NetworkIndexes;

    // public:
    //   ELLE_ATTRIBUTE_R(NetworkMap, users);

    //   /// Synchronise network.
    //   Network const&
    //   network_sync(Network const& network);

    //   /// Synchronise network.
    //   Network const&
    //   network_sync(std::string const& id);

    //   /// Retrieve a network.
    //   Network const&
    //   network(std::string const& id);

    //   /// Retrieve a network.
    //   Network const&
    //   network(uint32_t id);

    //   /// Create a new network.
    //   uint32_t
    //   network_create(std::string const& name,
    //                  bool auto_add = true);

    //   /// Prepare directories and files for the network to be launched.
    //   void
    //   network_prepare(std::string const& network_id);

    //   /// Delete a new network.
    //   std::string
    //   network_delete(std::string const& name,
    //                  bool force = false);

    //   /// Add a user to a network with its mail or id.
    //   void
    //   network_add_user(std::string const& network_id,
    //            std::string const& inviter_id,
    //            std::string const& user_id,
    //            std::string const& identity);

    //   void
    //   on_network_update_notification(
    //     infinit::oracles::trophonius::NetworkUpdateNotification const& notif);

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

      /*------------------.
      | Link Transactions |
      `------------------*/

      uint32_t
      create_link(std::vector<std::string> const& files,
                  std::string const& message);

      /*------------------.
      | Peer Transactions |
      `------------------*/

      uint32_t
      send_files(std::string const& peer_id,
                 std::vector<std::string> files,
                 std::string const& message);

      uint32_t
      start_onboarding(std::string const& file_path,
                       reactor::Duration const& transfer_duration = 5_sec);

    private:
      void
      _transactions_init();

      void
      _peer_transaction_resync();

      void
      _link_transaction_resync();

      void
      _transactions_clear();

      void
      _on_transaction_update(
        std::shared_ptr<infinit::oracles::Transaction> const& notif);

      void
      _on_peer_reachability_updated(
        infinit::oracles::trophonius::PeerReachabilityNotification const& notif);

    public:
      mutable reactor::MultiLockBarrier transaction_update_lock;
    /*--------------.
    | Configuration |
    `--------------*/
    public:
      struct Configuration
      {
        struct S3
        {
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
        int64_t max_mirror_size; // Copy files to send if below this size
        int64_t max_compress_size; // Only compresss archive if content below this size
        bool disable_upnp;
        typedef std::unordered_map<std::string, std::string> Features;
        Features features;
      };
      ELLE_ATTRIBUTE_RP(Configuration, configuration, protected:);
    private:
      void
      _apply_configuration(elle::json::Object json);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;
    };

    std::ostream&
    operator <<(std::ostream& out,
                NotificationType const& t);
  }
}

#include <surface/gap/State.hxx>

#endif
