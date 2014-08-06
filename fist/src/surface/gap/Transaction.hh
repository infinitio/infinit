#ifndef SURFACE_GAP_TRANSACTION_HH
# define SURFACE_GAP_TRANSACTION_HH

# include <stdint.h>

# include <string>
# include <set>
# include <unordered_set>

# include <boost/filesystem.hpp>

# include <infinit/oracles/Transaction.hh>
# include <surface/gap/Exception.hh>
# include <surface/gap/Notification.hh>
# include <surface/gap/enums.hh>
# include <surface/gap/fwd.hh>

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
          delete_,
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
      static std::set<infinit::oracles::Transaction::Status> final_statuses;

    /*---------.
    | Snapshot |
    `---------*/
    public:
      class Snapshot:
        public elle::Printable
      {
      public:
        Snapshot(
          bool sender,
          std::shared_ptr<Data> data,
          bool archived,
          boost::optional<std::vector<std::string>> files = {},
          boost::optional<std::string> message = {},
          boost::optional<std::string> plain_upload_uid = {}
          );
      public:
        ELLE_ATTRIBUTE_R(bool, sender);
        ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
        ELLE_ATTRIBUTE_R(boost::optional<std::vector<std::string>>, files);
        ELLE_ATTRIBUTE_R(boost::optional<std::string>, message);
        ELLE_ATTRIBUTE_R(boost::optional<std::string>, plain_upload_uid);
        ELLE_ATTRIBUTE_R(bool, archived);

      // Serialization
      public:
        Snapshot(elle::serialization::SerializerIn& serializer);
        void
        serialize(elle::serialization::Serializer& serializer);

      // Printable
      public:
        void
        print(std::ostream& stream) const override;
      };

    /*---------.
    | Snapshot |
    `---------*/
    // FIXME: generalize setting the data & saving the snapshot.
    // private:
    public:
      virtual
      void
      _snapshot_save() const;
      ELLE_ATTRIBUTE_R(boost::filesystem::path, snapshots_directory);
      ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_path);

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Construct from server data.
      Transaction(State& state,
                  uint32_t id,
                  std::shared_ptr<Data> data,
                  bool history = false);
      /// Construct from snapshot.
      Transaction(State& state,
                  uint32_t id,
                  Snapshot snapshot,
                  boost::filesystem::path snapshot_path);
      /// Construct as new for file sending.
      Transaction(surface::gap::State& state,
                  uint32_t id,
                  std::string const& peer_id,
                  std::vector<std::string> files,
                  std::string const& message);
      /// Construct as new for link generation.
      Transaction(surface::gap::State& state,
                  uint32_t id,
                  std::vector<std::string> files,
                  std::string const& message);
      /// Move.
      Transaction(Transaction&&) = default;
      /// Destruct.
      ~Transaction();
      ELLE_ATTRIBUTE_R(State&, state);
      ELLE_ATTRIBUTE(boost::optional<std::vector<std::string>>, files);
      ELLE_ATTRIBUTE(boost::optional<std::string>, message);
      ELLE_ATTRIBUTE_RW(boost::optional<std::string>, plain_upload_uid);
      ELLE_ATTRIBUTE_Rw(bool, archived);

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

      // Only for LinkTransactions.
      virtual
      void
      delete_();

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

    public:
      void
      on_transaction_update(std::shared_ptr<Data> data);

      void
      notify_user_connection_status(std::string const& user_id,
                                    bool user_status,
                                    std::string const& device_id,
                                    bool device_status);
      void
      notify_peer_reachable(std::vector<std::pair<std::string, int>> const& endpoints);
      void
      notify_peer_unreachable();

      // Reinitialize everything. Invoked when connection to servers is reset.
      virtual
      void
      reset();

    /*------------.
    | Attributes |
    `------------*/
    public:
      gap_TransactionStatus
      status() const;
      ELLE_ATTRIBUTE_R(uint32_t, id);
      ELLE_ATTRIBUTE_R(uint32_t, sender);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
    protected:
      std::unique_ptr<TransactionMachine> _machine;

    /*--------.
    | Helpers |
    `--------*/
    public:
      bool
      has_transaction_id(std::string const& id) const;
      bool
      final() const;
      /// Whether the transaction is over.
      ELLE_ATTRIBUTE_R(bool, over);
      /// Recorded lowest failure reason.
      ELLE_ATTRIBUTE_RW(std::string, failure_reason);
    private:
      friend class TransactionMachine;
      gap_TransactionStatus
      _transaction_status(Transaction::Data const& data) const;

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
