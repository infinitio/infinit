#ifndef SURFACE_GAP_TRANSACTION_MACHINE_HH
# define SURFACE_GAP_TRANSACTION_MACHINE_HH

# include <unordered_set>

# include <boost/filesystem.hpp>
# include <boost/signals2/signal.hpp>

# include <elle/Printable.hh>
# include <elle/serialize/construct.hh>

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/network/socket.hh>
# include <reactor/thread.hh>

# include <aws/Credentials.hh>
# include <aws/Exceptions.hh>
# include <aws/S3.hh>

# include <infinit/oracles/Transaction.hh>
# include <infinit/oracles/meta/Client.hh>
# include <oracles/src/infinit/oracles/PeerTransaction.hh>
# include <papier/fwd.hh>
# include <surface/gap/enums.hh>
# include <surface/gap/fwd.hh>
# include <surface/gap/TransferMachine.hh>

namespace surface
{
  namespace gap
  {
    class State;

    enum EncryptionLevel
    {
      EncryptionLevel_None = 0,
      EncryptionLevel_Weak = 1,
      EncryptionLevel_Strong = 2
    };
    class TransactionMachine:
      public elle::Printable
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef infinit::oracles::Transaction Data;
      typedef TransactionMachine Self;

    public:
      TransactionMachine(Transaction& transaction,
                         uint32_t id,
                         std::shared_ptr<TransactionMachine::Data> data);

      virtual
      ~TransactionMachine();

    protected:
      virtual
      void
      _save_snapshot() const;
    protected:
      /// Launch the reactor::chine at the given state.
      void
      _run(reactor::fsm::State& initial_state);

      /// Kill the reactor::Machine.
      void
      _stop();

      /// Report s3 error to metrics.
      void
      _report_s3_error(aws::AWSException const& exception, bool will_retry);
    public:
      /// Use to notify that the transaction has been updated on the remote.
      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) = 0;

      /// Notify that the peer is available for peer to peer connection.
      virtual
      void
      peer_available(std::vector<std::pair<std::string, int>> const& local_endpoints,
                     std::vector<std::pair<std::string, int>> const& public_endpoints);

      /// Notify that the peer is unavailable for peer to peer connection.
      virtual
      void
      peer_unavailable();

      /// Notify a user went online or offline on a device.
      virtual
      void
      notify_user_connection_status(std::string const& user_id,
                                    bool user_status,
                                    std::string const& device_id,
                                    bool device_status);

      /// Cancel the transaction.
      void
      cancel(std::string const& reason);

      /// Pause the transfer.
      /// XXX: Not implemented yet.
      virtual
      bool
      pause();

      /// For the transfer to roll back to the connection state.
      /// XXX: Not implemented yet.
      virtual
      void
      interrupt();

      /// Join the machine thread.
      /// The machine must be in his way to a final state, otherwise the caller
      /// will deadlock.
      virtual
      void
      join();

      /** Reset hypothetical running transfer
      * Invoked when some new information came that might invalidate what
      * transfer is doing (tropho connection reset for now).
      */
      void
      reset_transfer();

    public:
      /// Returns if the machine is releated to the given transaction id.
      bool
      has_id(uint32_t id);

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE_R(uint32_t, id);

    protected:
      reactor::fsm::Machine _machine;
      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, machine_thread);

    /*---------.
    | Snapshot |
    `---------*/
    public:
      class Snapshot
        : public elle::Printable
      {
      public:
        Snapshot(TransactionMachine const& machine);
        Snapshot(elle::serialization::SerializerIn& source);
        void
        serialize(elle::serialization::Serializer& s);
        static
        boost::filesystem::path
        path(TransactionMachine const& machine);
        ELLE_ATTRIBUTE_R(std::string, current_state);

      protected:
        virtual
        void
        print(std::ostream& stream) const override;
      };
      Snapshot
      snapshot() const;

    protected:
      friend class Snapshot;

      // XXX: not all transactions will need AWS credentials.
      virtual
      std::unique_ptr<infinit::oracles::meta::CloudCredentials>
      _cloud_credentials(bool regenerate) = 0;

    protected:
      void
      _another_device();
      virtual
      void
      _finish();
      void
      _finish(infinit::oracles::Transaction::Status status);
      void
      _reject();
      virtual
      void
      _fail();
      void
      _cancel();
      virtual
      void
      _transfer() = 0;
      void
      _finalize(infinit::oracles::Transaction::Status);
      // invoked to cleanup data when this transaction will never restart
      virtual
      void
      cleanup() = 0;
    private:
      void
      _clean();
      void
      _end();

    protected:
      // This state has to be protected to allow the children to start the
      // machine in this state.
      reactor::fsm::State& _another_device_state;
      reactor::fsm::State& _cancel_state;
      reactor::fsm::State& _end_state;
      reactor::fsm::State& _fail_state;
      reactor::fsm::State& _finish_state;
      reactor::fsm::State& _reject_state;
      reactor::fsm::State& _transfer_state;

    public:
      virtual
      bool
      completed() const = 0;
      ELLE_ATTRIBUTE_RX(reactor::Barrier, finished);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, rejected);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, canceled);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, failed);

      ELLE_ATTRIBUTE_RX(reactor::Signal, reset_transfer_signal);

    /*------------.
    | Transaction |
    `------------*/
    public:
      ELLE_ATTRIBUTE_R(surface::gap::Transaction&, transaction);
      ELLE_ATTRIBUTE_R(surface::gap::State&, state);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
    public:
      virtual
      float
      progress() const = 0;

    public:
      std::string const&
      transaction_id() const;

    protected:
      void
      transaction_id(std::string const& id);
      ELLE_ATTRIBUTE_RX(
        boost::signals2::signal<void (std::string const&)>, transaction_id_set);
      virtual
      bool
      concerns_this_device() = 0;
      virtual
      void
      _update_meta_status(infinit::oracles::Transaction::Status) = 0;

    public:
      virtual
      bool
      is_sender() const = 0;

    protected:
      friend class Transferer;
      friend class TransferMachine;
      void
      gap_status(gap_TransactionStatus);

    /*--------.
    | Metrics |
    `--------*/
    protected:
      virtual
      void
      _metrics_ended(infinit::oracles::Transaction::Status status,
                     std::string reason = "");

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };

    class S3
      : public aws::S3
    {
    public:
      S3(State& state,
         std::function<aws::Credentials(bool)> query_credentials);
      virtual
      aws::URL
      hostname(aws::Credentials const& credentials) const override;
      ELLE_ATTRIBUTE(State&, state);
    };
  }
}

# include <surface/gap/TransactionMachine.hxx>

#endif
