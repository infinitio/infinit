#ifndef SURFACE_GAP_TRANSFERMACHINE_HH
# define SURFACE_GAP_TRANSFERMACHINE_HH

# include <surface/gap/enums.hh>
# include <surface/gap/Notification.hh>
# include <plasma/fwd.hh>

# include <papier/fwd.hh>
# include <etoile/fwd.hh>

# include <hole/storage/Directory.hh>
# include <hole/implementations/slug/Slug.hh>

# include <nucleus/proton/Network.hh>

# include <metrics/fwd.hh>

# include <reactor/fsm.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/scheduler.hh>
# include <reactor/thread.hh>
# include <reactor/mutex.hh>
# include <reactor/waitable.hh>
# include <reactor/Barrier.hh>

# include <elle/Printable.hh>

# include <thread>

namespace surface
{
  namespace gap
  {
    class State;

    class TransferMachine:
      public elle::Printable
    {
    public:
      class Notification:
        public surface::gap::Notification
      {
      public:
        static surface::gap::Notification::Type type;

        Notification(uint32_t id, TransferState status);

        uint32_t id;
        TransferState status;
      };

    public:
      typedef plasma::Transaction Data;
    public:
      TransferMachine(surface::gap::State const& state,
                      uint32_t id,
                      std::shared_ptr<TransferMachine::Data> transaction);

      virtual
      ~TransferMachine();
    protected:
      /// Launch the reactor::Machine at the given state.
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
      /// Returns if the machine is releated to the given network object id.
      bool
      concerns_network(std::string const& network_id);

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
      _finiliaze(plasma::TransactionStatus);

    private:
      void
      _local_clean();

      void
      _remote_clean();

      void
      _clean();

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
      reactor::fsm::State& _remote_clean_state;
      reactor::fsm::State& _local_clean_state;

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

      void
      _publish_interfaces();

      void
      _connection();

      void
      _wait_for_peer();

      void
      _transfer();

      void
      _core_stoped();

      // Common on both sender and recipient process.
      ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
      // Broken: I you disconnect, which is quasi transparent, you must publish your interfaces.
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_peer_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, transfer_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, core_stoped_state);

    protected:
      // User status signal.
      reactor::Signal _peer_online;
      reactor::Signal _peer_offline;

      // Slug?
      reactor::Signal _peer_connected;
      reactor::Signal _peer_disconnected;

      ELLE_ATTRIBUTE_r(float, progress);
      ELLE_ATTRIBUTE_P(reactor::Mutex, progress_mutex, mutable);
      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, pull_progress_thread);

      void
      _retrieve_progress();

      ELLE_ATTRIBUTE_R(surface::gap::State const&, state);

      /*------------.
      | Transaction |
      `------------*/
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);

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

    protected:
      std::string const&
      network_id() const;

      void
      network_id(std::string const& id);

      ELLE_ATTRIBUTE(std::unique_ptr<nucleus::proton::Network>, network);
    protected:
      nucleus::proton::Network&
      network();

      ELLE_ATTRIBUTE(std::unique_ptr<papier::Descriptor>, descriptor);
    protected:
      papier::Descriptor const&
      descriptor();

      /*-------.
      | Etoile |
      `-------*/
      ELLE_ATTRIBUTE(std::unique_ptr<hole::storage::Directory>, storage);
    protected:
      hole::storage::Directory&
      storage();

      ELLE_ATTRIBUTE(std::unique_ptr<hole::implementations::slug::Slug>, hole);
    protected:
      hole::implementations::slug::Slug&
      hole();

      ELLE_ATTRIBUTE(std::unique_ptr<etoile::Etoile>, etoile);
    protected:
      etoile::Etoile&
      etoile();

    public:

      virtual
      std::string
      type() const;

      /*--------.
      | Metrics |
      `--------*/
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
  }
}

#endif
