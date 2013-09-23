#ifndef SURFACE_GAP_TRANSFERMACHINE_HH
# define SURFACE_GAP_TRANSFERMACHINE_HH

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>

//# include <plasma/fwd.hh>
# include <plasma/plasma.hh>

# include <metrics/fwd.hh>
# include <papier/fwd.hh>

# include <frete/Frete.hh>
# include <station/fwd.hh>

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/mutex.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/scheduler.hh>
# include <reactor/thread.hh>
# include <reactor/waitable.hh>

# include <elle/Printable.hh>
# include <elle/serialize/construct.hh>

# include <boost/filesystem.hpp>

# include <thread>
# include <unordered_set>

namespace surface
{
  namespace gap
  {
    class State;

    class TransferMachine:
      public elle::Printable
    {
    public:
      typedef plasma::Transaction Data;

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

        None = 99,
      };

    public:
      class Snapshot
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
      };

    public:
      TransferMachine(surface::gap::State const& state,
                      uint32_t id,
                      std::shared_ptr<TransferMachine::Data> transaction);

      virtual
      ~TransferMachine();

    private:
      ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_path);

    protected:
      virtual
      Snapshot
      _make_snapshot() const;
    private:
      void
      _save_snapshot() const;

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
      transaction_status_update(plasma::TransactionStatus status) = 0;

      /// Use to notify that the peer status changed to connected or
      /// disconnected.
      void
      peer_connection_update(bool user_status);

      /// Cancel the transaction.
      void
      cancel();

      /// Join the machine thread.
      /// The machine must be in his way to a final state, otherwise the caller
      /// will deadlock.
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

      void
      _finalize(plasma::TransactionStatus);

    private:
      void
      _clean();

      void
      _end();

    protected:
      virtual
      void
      _transfer_operation() = 0;

    protected:
      // This state has to be protected to allow the children to start the
      // machine in this state.
      reactor::fsm::State& _transfer_core_state;
      reactor::fsm::State& _finish_state;
      reactor::fsm::State& _reject_state;
      reactor::fsm::State& _cancel_state;
      reactor::fsm::State& _fail_state;
      reactor::fsm::State& _end_state;

    protected:
      reactor::Barrier _finished;
      reactor::Barrier _rejected;
      reactor::Barrier _canceled;
      reactor::Barrier _failed;

      /*-------------.
      | Core Machine |
      `-------------*/
    private:
      reactor::fsm::Machine _core_machine;
      std::unique_ptr<reactor::Thread> _core_machine_thread;

      void
      _publish_interfaces();

      std::unique_ptr<station::Host>
      _connect();

      void
      _connection();

      void
      _wait_for_peer();

      void
      _transfer();

      void
      _core_stoped();

      void
      _core_paused();

      // Common on both sender and recipient process.
      ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
      // XXX: I you disconnect, which is quasi transparent, you must publish your interfaces.
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_peer_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, transfer_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, core_stoped_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, core_paused_state);

    protected:
      // User status signal.
      reactor::Barrier _peer_online;
      reactor::Barrier _peer_offline;

      // Slug?
      reactor::Signal _peer_connected;
      reactor::Signal _peer_disconnected;

      ELLE_ATTRIBUTE_R(surface::gap::State const&, state);

      /*------------.
      | Transaction |
      `------------*/
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
      ELLE_ATTRIBUTE_r(float, progress);

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

      std::unique_ptr<station::Host> _host;

    public:
      virtual
      std::string
      type() const;

    protected:
      std::unique_ptr<infinit::protocol::Serializer> _serializer;
      std::unique_ptr<infinit::protocol::ChanneledStream> _channels;
      std::unique_ptr<frete::Frete> _frete;

    protected:
      virtual
      frete::Frete&
      frete() = 0;

      /*--------.
      | Metrics |
      `--------*/
    public:
      metrics::Metric
      network_metric() const;

      metrics::Metric
      transaction_metric() const;

      /*----------.
      | Printable |
      `----------*/
      void
      print(std::ostream& stream) const override;
    };


    std::ostream&
    operator <<(std::ostream& out,
                TransferMachine::State const& t);
  }
}

# include <surface/gap/TransferMachine.hxx>

#endif
