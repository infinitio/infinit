#ifndef TRANSACTIONMANAGER_HH
# define TRANSACTIONMANAGER_HH

# include <plasma/plasma.hh>
# include <unordered_set>
# include <surface/gap/status.hh>

# include <surface/gap/NetworkManager.hh>
# include <surface/gap/NotificationManager.hh>
# include <surface/gap/OperationManager.hh>
# include <surface/gap/UserManager.hh> // Could be avoid using accept response.
# include <surface/gap/metrics.hh>

# include <plasma/trophonius/Client.hh>
# include <plasma/meta/Client.hh>

namespace surface
{
  namespace gap
  {
    /*-------.
    | Usings |
    `-------*/
    using ::plasma::Transaction;
    using Self = ::plasma::meta::SelfResponse;
    using Device = ::plasma::meta::Device;
    using NotificationManager = ::surface::gap::NotificationManager;
    using NetworkManager = ::surface::gap::NetworkManager;
    using UserManager = ::surface::gap::UserManager;

    class TransactionManager:
      public OperationManager,
      public Notifiable
    {
      /*-----------.
      | Attributes |
      `-----------*/
    private:
      NetworkManager& _network_manager;
      UserManager& _user_manager;
      // XXX: meta should be constant everywhere.
      // But httpclient fire can't be constant.
      plasma::meta::Client& _meta;
      elle::metrics::Reporter& _reporter;
      Self& _self;
      Device _device;
      ELLE_ATTRIBUTE_R(std::string, output_dir);

      /*-------------.
      | Construction |
      `-------------*/
    public:
      TransactionManager(NotificationManager& notification_manager,
                         NetworkManager& network_manager,
                         UserManager& user_manager,
                         plasma::meta::Client& meta,
                         elle::metrics::Reporter& reporter,
                         Self& self,
                         Device const& device);

      virtual
      ~TransactionManager();

      /// @brief Clear the current manager and his sons.
      void
      clear();

      /// @brief Set output dir.
      void
      output_dir(std::string const& dir);

      /*----------------------.
      | Sending and recieving |
      `----------------------*/
    public:
      /// @brief Send a file list to a specified user.
      ///
      /// Create a network, copy files locally, create transaction.
      OperationManager::OperationId
      send_files(std::string const& recipient_id_or_email,
                 std::unordered_set<std::string> const& files);

    private:
      /*---------.
      | Progress |
      `---------*/
      struct TransactionProgress;
      typedef std::unique_ptr<TransactionProgress> TransactionProgressPtr;
      typedef std::map<std::string, TransactionProgressPtr> TransactionProgressMap;
      elle::threading::Monitor<TransactionProgressMap> _progresses;

    public:
      /// @brief Returns a floating number in [0.0f, 1.0f]
      float
      progress(std::string const& transaction_id);


      /*--------.
      | Storage |
      `--------*/
    private:
      typedef std::map<std::string, plasma::Transaction> TransactionsMap;
      typedef std::unique_ptr<TransactionsMap> TransactionMapPtr;
      elle::threading::Monitor<TransactionMapPtr> _transactions;

    public:
      /// @brief Pull transactions from serveur.
      TransactionsMap const&
      all();

      /// @brief Get data from a specific transaction.
      Transaction const&
      one(std::string const& transaction_id);

      /// @brief Fetch transaction from server (and update it).
      Transaction const&
      sync(std::string const& id);

    private:
      /// @brief Ensure the transaction belongs to the user, as sender or
      /// recipient. May also check if involved devices are the good ones.
      void
      _ensure_ownership(Transaction const& transaction,
                        bool check_devices = false);

      /*-------------------.
      | Transaction update |
      `-------------------*/
    private:
      struct State
      {
        enum
        {
          none,     // Unknown transaction.
          preparing,
          running,
        } state;
        int tries;
        OperationId operation;
        State():
          state{none},
          tries{0},
          operation{0}
        {}
      };
      typedef std::map<std::string, State> StateMap;
      elle::threading::Monitor<StateMap> _states;

    public:
      /// @brief Update transaction status.
      void
      update(std::string const& transaction_id,
             plasma::TransactionStatus status);

      void
      accept_transaction(Transaction const& transaction);

      void
      cancel_transaction(Transaction const& transaction);

    private:
      void
      _accept_transaction(Transaction const& transaction,
                          Operation& operation);
    private:
      void
      _prepare_upload(Transaction const& transaction);

      void
      _start_upload(Transaction const& transaction);

      void
      _start_download(Transaction const& transaction);

      /*----------.
      | Callbacks |
      `----------*/
    private:
      /// @brief Callback when recieving an new transaction.
      void
      _on_transaction(TransactionNotification const& notif, bool is_new);
    };
  }
}

#endif
