#ifndef SURFACE_GAP_TRANSACTION_MACHINE_HH
# define SURFACE_GAP_TRANSACTION_MACHINE_HH

# include <unordered_set>

# include <boost/filesystem.hpp>

# include <elle/Printable.hh>
# include <elle/serialize/construct.hh>

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/network/socket.hh>
# include <reactor/thread.hh>

# include <frete/Frete.hh>
# include <infinit/oracles/Transaction.hh>
# include <papier/fwd.hh>
# include <station/fwd.hh>

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>
# include <surface/gap/TransferMachine.hh>

namespace surface
{
  namespace gap
  {
    class State;

    class TransactionMachine:
      public elle::Printable
    {
    public:
      typedef infinit::oracles::Transaction Data;

      // The values of the enums are used in the snapshot.
      // To add a state, add it a the end to avoid the invalidation of the locally
      // stored snapshots.
      enum class State
      {
        NewTransaction = 0,
        SenderCreateTransaction = 2,
        SenderWaitForDecision = 4,
        RecipientWaitForDecision = 5,
        RecipientAccepted = 6,
        PublishInterfaces = 9,
        Connect = 10,
        PeerDisconnected = 11,
        PeerConnectionLost = 12,
        Transfer = 13,
        Finished = 16,
        Rejected = 17,
        Canceled = 18,
        Failed = 19,
        Over = 20,
        CloudBuffered = 21,
        CloudBufferingBeforeAccept = 22,
        GhostCloudBuffering = 23,
        GhostCloudBufferingFinished = 24,
        DataExhausted = 25,
        CloudSynchronize = 26,
        None = 99,
      };

    public:
      class Snapshot:
        public elle::Printable
      {
      public:
        Snapshot(Data const& data,
                 State const state,
                 std::unordered_set<std::string> const& files = {},
                 std::string const& message = "");

        ELLE_SERIALIZE_CONSTRUCT(Snapshot){}

      public:
        Data data;
        State state;
        std::unordered_set<std::string> files;
        std::string message;
        bool archived;

        /*----------.
        | Printable |
        `----------*/
      public:
        void
        print(std::ostream& stream) const override;
      };

    public:
      TransactionMachine(surface::gap::State const& state,
                         uint32_t id,
                         std::shared_ptr<TransactionMachine::Data> transaction);

      virtual
      ~TransactionMachine();

    public:
      ELLE_ATTRIBUTE_R(boost::filesystem::path, snapshot_path);

    protected:
      virtual
      Snapshot
      _make_snapshot() const;
      virtual
      void
      _save_snapshot() const;
    protected:
      virtual
      void
      _save_transfer_snapshot() = 0;
    protected:
      /// Launch the reactor::chine at the given state.
      void
      _run(reactor::fsm::State& initial_state);

      /// Kill the reactor::Machine.
      void
      _stop();

    public:
      /// Use to notify that the transaction has been updated on the remote.
      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) = 0;

      /// Use to notify that the peer is available for peer to peer connection.
      void
      peer_availability_changed(bool added);

      /// Use to notify that the peer status changed to connected or
      /// disconnected.
      void
      peer_connection_changed(bool user_status);

      /// Cancel the transaction.
      void
      cancel();

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

    public:
      /// Returns if the machine is releated to the given transaction object id.
      bool
      concerns_transaction(std::string const& transaction_id);

      /// Returns if the machine is releated to the given user object id.
      bool
      concerns_user(std::string const& user_id);

      /// Returns if the machine is releated to the given device object id.
      bool
      concerns_device(std::string const& device_id);

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

    protected:
      // XXX: Remove when ELLE_ATTRIBUTE handle protected methods.
      State _current_state;
      ELLE_ATTRIBUTE_X(reactor::Signal, state_changed);

    protected:
      friend class Transferer;
      friend class TransferMachine;
      void
      current_state(State const& state);

    public:
      State
      current_state() const;

    protected:
      void
      _transfer_core();

      void
      _finish();

      void
      _reject();

      void
      _fail();

      void
      _cancel();

      virtual
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
      virtual
      void
      _transfer_operation(frete::RPCFrete& frete) = 0;
      virtual
      // Go all the way to the cloud until interrupted.
      void
      _cloud_operation() = 0;
      virtual
      // Just synchronize what you can with cloud
      void
      _cloud_synchronize() = 0;
    protected:
      // This state has to be protected to allow the children to start the
      // machine in this state.
      reactor::fsm::State& _transfer_core_state;
      reactor::fsm::State& _finish_state;
      reactor::fsm::State& _reject_state;
      reactor::fsm::State& _cancel_state;
      reactor::fsm::State& _fail_state;
      reactor::fsm::State& _end_state;

    public:
      ELLE_ATTRIBUTE_RX(reactor::Barrier, finished);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, rejected);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, canceled);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, failed);

    /*-------------.
    | Core Machine |
    `-------------*/
    protected:
      std::unique_ptr<Transferer> _transfer_machine;

    /*------------.
    | Transaction |
    `------------*/
    public:
      ELLE_ATTRIBUTE_R(surface::gap::State const&, state);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
      ELLE_ATTRIBUTE(float, progress);
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

    public:
      std::string const&
      peer_id() const;

    protected:
      void
      peer_id(std::string const& id);

    public:
      virtual
      bool
      is_sender() const = 0;

      ELLE_ATTRIBUTE(std::unique_ptr<station::Station>, station);
    protected:
      station::Station&
      station();

    public:
      virtual
      std::string
      type() const;

    protected:
      virtual
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& socket) = 0;

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };


    std::ostream&
    operator <<(std::ostream& out,
                TransactionMachine::State const& t);
  }
}

# include <surface/gap/TransactionMachine.hxx>

#endif
