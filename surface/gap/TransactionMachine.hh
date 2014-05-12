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
# include <surface/gap/fwd.hh>
# include <surface/gap/TransferMachine.hh>

# include <aws/Credentials.hh>

namespace surface
{
  namespace gap
  {
    class State;

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


      /// Notify that the peer is available for peer to peer connection.
      void
      peer_available(std::vector<std::pair<std::string, int>> const& endpoints);

      /// Notify that the peer is unavailable for peer to peer connection.
      void
      peer_unavailable();

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

      /** Reset hypothetical running transfer
      * Invoked when some new information came that might invalidate what
      * transfer is doing (tropho connection reset for now).
      */
      void
      reset_transfer();

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

    protected:
      friend class Transferer;
      friend class PeerTransferMachine;
      friend class Snapshot;

      std::function<aws::Credentials(bool)>
      make_aws_credentials_getter();

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

      ELLE_ATTRIBUTE_RX(reactor::Signal, reset_transfer_signal);

    /*--------.
    | Station |
    `--------*/
      ELLE_ATTRIBUTE(std::unique_ptr<station::Station>, station);
    protected:
      station::Station&
      station();

    /*-------------.
    | Core Machine |
    `-------------*/
    protected:
      std::unique_ptr<Transferer> _transfer_machine;

    /*------------.
    | Transaction |
    `------------*/
    public:
      ELLE_ATTRIBUTE_R(surface::gap::Transaction&, transaction);
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

    protected:
      virtual
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& socket) = 0;

    protected:
      void
      gap_state(gap_TransactionStatus state);

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };
  }
}

# include <surface/gap/TransactionMachine.hxx>

#endif
