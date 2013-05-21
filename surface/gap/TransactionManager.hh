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

    class TransactionManager: public OperationManager, Notifiable
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
      /// @brief Start the transfer process on recipient.
      ///
      OperationId
      _download_files(std::string const& transaction_id);

      /*---------.
      | Progress |
      `---------*/
      struct TransactionProgress;
      typedef std::unique_ptr<TransactionProgress> TransactionProgressPtr;
      std::map<std::string, TransactionProgressPtr> _progresses;

    public:
      /// @brief Returns a floating number in [0.0f, 1.0f]
      float
      progress(std::string const& transaction_id);


      /*--------.
      | Storage |
      `--------*/
    private:
      typedef std::map<std::string, plasma::Transaction> TransactionsMap;
      std::unique_ptr<TransactionsMap> _transactions;

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
      | Transcation update |
      `-------------------*/
    public:
      /// @brief Update transaction status.
      void
      update(std::string const& transaction_id,
             gap_TransactionStatus status);

    private:
      /// @brief Use to launch the process if the recipient already accepted
      // the transaction.
      void
      _create_transaction(Transaction const& transaction);

      /// @brief Use to accept the transaction for the recipient.
      void
      _accept_transaction(Transaction const& transaction);

      /// @brief Use to inform recipient that everything is ok and he can
      /// prepare for downloading.
      void
      _prepare_transaction(Transaction const& transaction);

      /// @brief Use to inform recipient that everything is ok and he can start
      /// downloading.
      void
      _start_transaction(Transaction const& transaction);

      /// @brief Use to inform the sender that download is complete.
      void
      _close_transaction(Transaction const& transaction);

      /// @brief Use to cancel a pending transaction or an unfinished one.
      void
      _cancel_transaction(Transaction const& transaction);

      /*----------.
      | Callbacks |
      `----------*/
    private:
      /// @brief Callback when recieving an new transaction.
      void
      _on_transaction(TransactionNotification const& notif, bool is_new);

      /// @brief Calback when recieving an update for a transaction.
      void
      _on_transaction_status(TransactionStatusNotification const& notif);

      /// @brief Use to created the transaction for the recipient.
      void
      _on_transaction_created(Transaction const& transaction);

      /// @brief Use to add rights on network when the recipient accepts.
      void
      _on_transaction_accepted(Transaction const& transaction);

      /// @brief Use to .
      void
      _on_transaction_prepared(Transaction const& transaction);

      /// @brief Use to .
      void
      _on_transaction_started(Transaction const& transaction);

      /// @brief Use to close network.
      void
      _on_transaction_closed(Transaction const& transaction);

      /// @brief Use to destroy network if transaction has been canceled.
      void
      _on_transaction_canceled(Transaction const& transaction);
    };
  }
}

#endif
