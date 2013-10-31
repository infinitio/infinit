#ifndef  SURFACE_GAP_STATE_HH
# define SURFACE_GAP_STATE_HH

# include "Device.hh"
# include "Exception.hh"
# include "Self.hh"
# include "gap.h"
# include <surface/gap/Notification.hh>
# include <surface/gap/Transaction.hh>
# include <surface/gap/metrics.hh>
# include <surface/gap/Transaction.hh>

# include <common/common.hh>

# include <infinit/oracles/Transaction.hh>
# include <infinit/oracles/meta/Client.hh>
# include <infinit/oracles/trophonius/Client.hh>

# include <papier/Passport.hh>
# include <papier/Identity.hh>

# include <reactor/scheduler.hh>
# include <reactor/thread.hh>
# include <reactor/mutex.hh>

# include <elle/format/json/fwd.hh>
# include <elle/threading/Monitor.hh>
# include <elle/Printable.hh>

# include <map>
# include <string>
# include <exception>
# include <unordered_set>
# include <mutex>

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
    };

    class State:
      public elle::Printable
    {
    public:
      /*-------.
      | Logger |
      `-------*/
      // XXX: LoggerInitializer must be the first member of state.
      // His construction force the instanciation of the logger.
      LoggerInitializer _logger_intializer;

      /*--------.
      | Servers |
      `--------*/
      ELLE_ATTRIBUTE(infinit::oracles::meta::Client, meta);
      ELLE_ATTRIBUTE_R(infinit::oracles::trophonius::Client, trophonius);

      infinit::oracles::meta::Client const&
      meta(bool authentication_required = true) const;

      /*----------.
      | Reporters |
      `----------*/
      ELLE_ATTRIBUTE_P(metrics::Reporter, reporter, mutable);
      ELLE_ATTRIBUTE_P(metrics::Reporter, google_reporter, mutable);
      ELLE_ATTRIBUTE_P(metrics::Reporter, infinit_transaction_reporter, mutable);
      ELLE_ATTRIBUTE_P(metrics::Reporter, infinit_user_reporter, mutable);
      ELLE_ATTRIBUTE_P(metrics::Reporter, mixpanel_reporter, mutable);

    public:
      metrics::Reporter&
      reporter() const
      {
        return this->_reporter;
      }

      metrics::Reporter&
      mixpanel_reporter() const
      {
        return this->_mixpanel_reporter;
      }

      metrics::Reporter&
      google_reporter() const
      {
        return this->_google_reporter;
      }

      metrics::Reporter&
      infinit_transaction_reporter() const
      {
        return this->_infinit_transaction_reporter;
      }

      metrics::Reporter&
      infinit_user_reporter() const
      {
        return this->_infinit_user_reporter;
      }

      /*-------------.
      | Construction |
      `-------------*/
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

    public:
      State(std::string const& meta_host = common::meta::host(),
            uint16_t meta_port = common::meta::port(),
            std::string const& trophonius_host = common::trophonius::host(),
            uint16_t trophonius_port = common::trophonius::port(),
            std::string const& apertus_host = common::apertus::host(),
            uint16_t apertus_port = common::apertus::port());
      ~State();

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

      ELLE_ATTRIBUTE(reactor::Mutex, login_mutex);
    public:
      /// Login to meta.
      void
      login(std::string const& email,
            std::string const& password);

      /// Logout from meta.
      void
      logout();

      void
      register_(std::string const& fullname,
                std::string const& email,
                std::string const& password,
                std::string const& activation_code);

      ///
      static
      std::string
      hash_password(std::string const& email,
                    std::string const& password);

      std::string
      user_directory();

      std::string const&
      token_generation_key() const;

      ELLE_ATTRIBUTE_Rw(std::string, output_dir);
    private:
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
      on_connection_changed(bool connection_status);

      void
      handle_notification(
        std::unique_ptr<infinit::oracles::trophonius::Notification>&& notification);

      // For lisibility purpose, papiers, user, network and transaction methods
      // are located in specific files in _detail.
      /*--------.
      | Papiers |
      `--------*/
    private:
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

      /// Create or update the local device.
      void
      update_device(std::string const& name,
                    bool force_create = false) const;

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

    public:
      typedef infinit::oracles::meta::User User;
      typedef std::pair<const uint32_t, User> UserPair;
      typedef std::unordered_map<uint32_t, User> UserMap;
      typedef std::unordered_set<User> Users;
      typedef std::map<std::string, uint32_t> UserIndexMap;
      typedef std::unordered_set<uint32_t> UserIndexes;

      ELLE_ATTRIBUTE_RP(UserMap, users, mutable);
      ELLE_ATTRIBUTE_RP(UserIndexMap, user_indexes, mutable);
      ELLE_ATTRIBUTE_RP(UserIndexes, swagger_indexes, mutable);
      ELLE_ATTRIBUTE(reactor::Mutex, swagger_mutex);

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
      user_from_public_key(std::string const& public_key) const;

      void
      _user_resync();

      UserIndexes
      user_search(std::string const& text) const;

      bool
      device_status(std::string const& user_id,
                    std::string const& device_id) const;

      std::string
      invite(std::string const& email);

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
      _on_new_swagger(infinit::oracles::trophonius::NewSwaggerNotification const& notif);

      void
      _on_swagger_status_update(infinit::oracles::trophonius::UserStatusNotification const& notif);

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

      // /*-------------.
      // | Transactions |
      // `-------------*/
      class TransactionNotFoundException:
        public Exception
      {
      public:
        TransactionNotFoundException(uint32_t id);
        TransactionNotFoundException(std::string const& id);
      };

      // typedef std::unique_ptr<Transaction> TransactionPtr;
      typedef std::pair<uint32_t, Transaction> TransactionPair;
      typedef std::pair<const uint32_t, Transaction> TransactionConstPair;
      typedef std::unordered_map<uint32_t, Transaction> Transactions;
      typedef std::map<std::string, uint32_t> TransactionIndexMap;
      typedef std::unordered_set<uint32_t> TransactionIndexes;

      ELLE_ATTRIBUTE_R(Transactions, transactions);
      Transactions& transactions() {  return this->_transactions; }

      uint32_t
      send_files(std::string const& peer_id,
                 std::unordered_set<std::string>&& files,
                 std::string const& message);

    private:
      void
      _transactions_init();

      void
      _transaction_resync();

      void
      _transactions_clear();

      void
      _on_transaction_update(infinit::oracles::Transaction const& notif);

      void
      _on_peer_connection_update(
        infinit::oracles::trophonius::PeerConnectionUpdateNotification const& notif);

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
