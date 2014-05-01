#ifndef SURFACE_GAP_TRANSACTION_HH
# define SURFACE_GAP_TRANSACTION_HH

# include <surface/gap/Notification.hh>
# include <surface/gap/enums.hh>
# include <surface/gap/fwd.hh>
# include <surface/gap/Exception.hh>
# include <surface/gap/TransactionMachine.hh>

# include <infinit/oracles/trophonius/Client.hh>

# include <unordered_set>

# include <stdint.h>
# include <string>

namespace surface
{
  namespace gap
  {
    class Transaction:
      public elle::Printable
    {
      // - Exception -----------------------------------------------------------
      class BadOperation:
        public Exception
      {
      public:
        enum class Type
        {
          accept,
          reject,
          join,
          cancel,
          progress,
          pause,
          interrupt,
       };

        BadOperation(Type type);

        ELLE_ATTRIBUTE_R(Type, type);
      };

    public:
      class Notification:
        public surface::gap::Notification
      {
      public:
        static surface::gap::Notification::Type type;

        Notification(uint32_t id, gap_TransactionStatus status);

        uint32_t id;
        gap_TransactionStatus status;
      };

    public:
      typedef infinit::oracles::Transaction Data;
      static std::vector<infinit::oracles::Transaction::Status> final_statuses;

    public:
      Transaction(State const& state,
                  uint32_t id,
                  Data&& data,
                  bool history = false);

      Transaction(State const& state,
                  uint32_t id,
                  TransactionMachine::Snapshot data);

      Transaction(Transaction&&) = default;

      Transaction(surface::gap::State const& state,
                  uint32_t id,
                  std::string const& peer_id,
                  std::unordered_set<std::string>&& files,
                  std::string const& message);

      ~Transaction();

    public:
      virtual
      void
      accept();

      virtual
      void
      reject();

      virtual
      void
      cancel();

      virtual
      void
      join();

      virtual
      float
      progress() const;

      virtual
      bool
      pause();

      virtual
      void
      interrupt();

    protected:
      bool
      last_status(gap_TransactionStatus);

      void
      _notify_on_status_update(surface::gap::State const& state);

    public:
      void
      on_transaction_update(Data const& data);

      void
      on_peer_reachability_updated(
        infinit::oracles::trophonius::PeerReachabilityNotification const& update);

      void
      on_peer_connection_status_updated(
        infinit::oracles::trophonius::UserStatusNotification const& update);

      void
      peer_connection_status(bool status);

      void
      peer_available(std::vector<std::pair<std::string, int>> const& endpoints);

      void
      peer_unavailable();

      // Reinitialize everything. Invoked when connection to servers is reset.
      virtual
      void
      reset(surface::gap::State const& state);
      /*------------.
      | Atttributes |
      `------------*/
      ELLE_ATTRIBUTE_R(uint32_t, id);
      ELLE_ATTRIBUTE_R(uint32_t, sender);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
    protected:
      std::unique_ptr<TransactionMachine> _machine;
      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, machine_state_thread);
      ELLE_ATTRIBUTE_r(gap_TransactionStatus, last_status);
      /*--------.
      | Helpers |
      `--------*/
    public:
      bool
      concerns_user(std::string const& peer_id) const;

      bool
      is_sender(std::string const& user_id) const;

      bool
      concerns_device(std::string const& device_id) const;

      bool
      has_transaction_id(std::string const& id) const;

      bool
      final() const;
    private:
      gap_TransactionStatus
      _transaction_status(Transaction::Data const& data,
                          TransactionMachine::State state) const;

      /*----------.
      | Printable |
      `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;
    };
  }
}

#include "Transaction.hxx"

#endif
