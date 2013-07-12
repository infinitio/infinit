#ifndef SURFACE_GAP_TRANSACTIONMANAGER_HH
# define SURFACE_GAP_TRANSACTIONMANAGER_HH

# include "Device.hh"
# include "NetworkManager.hh"
# include "NotificationManager.hh"
# include "OperationManager.hh"
# include "Self.hh"
# include "Transaction.hh"
# include "TransactionStateMachine.hh"
# include "UserManager.hh"
# include "metrics.hh"
# include "status.hh"

# include <metrics/fwd.hh>

# include <plasma/trophonius/Client.hh>
# include <plasma/meta/Client.hh>

# include <elle/attribute.hh>

# include <string>
# include <unordered_set>
# include <reactor/scheduler.hh>

namespace surface
{
  namespace gap
  {
    class TransactionManager:
      public OperationManager,
      public Notifiable
    {
      /*-----------.
      | Attributes |
      `-----------*/
    private:
      typedef std::function<Self const&()> SelfGetter;
      typedef std::function<Device const&()> DeviceGetter;
      typedef std::function<void(unsigned int)> UpdateRemainingInvitations;

    private:
      NetworkManager& _network_manager;
      UserManager& _user_manager;
      // XXX: meta should be constant everywhere.
      // But httpclient fire can't be constant.
      plasma::meta::Client& _meta;
      metrics::Reporter& _reporter;

      ELLE_ATTRIBUTE(reactor::Scheduler&, scheduler);
      ELLE_ATTRIBUTE(SelfGetter, self);
      ELLE_ATTRIBUTE(DeviceGetter, device);
      ELLE_ATTRIBUTE(UpdateRemainingInvitations, update_remaining_invitations);
      ELLE_ATTRIBUTE_R(std::string, output_dir);
      ELLE_ATTRIBUTE(TransactionStateMachine, state_machine);

      /*-------------.
      | Construction |
      `-------------*/
    public:
      TransactionManager(
        reactor::Scheduler& scheduler,
        NotificationManager& notification_manager,
        NetworkManager& network_manager,
        UserManager& user_manager,
        plasma::meta::Client& meta,
        metrics::Reporter& reporter,
        SelfGetter const& self,
        DeviceGetter const& device,
        UpdateRemainingInvitations const& update_remaining_invitations);

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
      // Remove processes, states and operations related to a transaction.
      void
      _clean_transaction(Transaction const& transaction);

      /*-------------------.
      | Transaction update |
      `-------------------*/
    private:
      struct State
      {
        enum
        {
          none,     // Unknown transaction.
          accepting,
          preparing,
          running,
          finished,
          canceled,
        } state;
        int tries;
        OperationId operation;
        std::unordered_set<std::string> files;
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
      accept_transaction(std::string const& transaction_id);

      void
      cancel_transaction(std::string const& transaction_id);

    private:

      void
      _cancel_transaction(Transaction const& transaction);

      void
      _on_cancel_transaction(Transaction const& transaction);

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
      _on_transaction(plasma::Transaction const& notif);
      void
      _on_user_status(UserStatusNotification const& notif);
    };
  }
}

#endif
