#ifndef SURFACE_GAP_TRANSFER_MACHINE_HH
# define SURFACE_GAP_TRANSFER_MACHINE_HH

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>

# include <surface/gap/fwd.hh>

namespace surface
{
  namespace gap
  {
    /// Base class for a Transfer handler
    class BaseTransferer: public elle::Printable
    {
    public:
      typedef std::vector<std::pair<std::string, int>> Endpoints;
      BaseTransferer(TransactionMachine& owner);

      /// Run the transfer, return when finished
      virtual
      void
      run() = 0;

      virtual
      float
      progress() const;

      virtual
      bool
      finished() const;

      /// Notify the peer is available for peer to peer connection.
      virtual void
      peer_available(Endpoints const& endpoints) =0;
      /// Notify the peer is unavailable for peer to peer connection.
      virtual void
      peer_unavailable() = 0;
    protected:
      // Connection status of the peer according to the servers.
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_online);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_offline);
      TransactionMachine& _owner;
    };

    /// Transferer delegating work to a TransactionMachine owner, with
    /// a fixed FSM.
    class Transferer
      : public BaseTransferer
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      Transferer(TransactionMachine& owner);
      virtual
      ~Transferer() = default;

    /*--------.
    | Control |
    `--------*/
    public:
      void
      run() override;

    protected:
      reactor::fsm::Machine _fsm;

    /*---------.
    | Triggers |
    `---------*/
    public:
      /// Notify the peer is available for peer to peer connection.
      void
      peer_available(std::vector<std::pair<std::string, int>> const& endpoints) override;
      /// Notify the peer is unavailable for peer to peer connection.
      void
      peer_unavailable() override;

    private:
      // Availability of the peer for peer to peer connection.
      ELLE_ATTRIBUTE(reactor::Barrier, peer_reachable);
      ELLE_ATTRIBUTE(reactor::Barrier, peer_unreachable);
      // The peer endpoints.
      typedef std::vector<std::pair<std::string, int>> Endpoints;
      ELLE_ATTRIBUTE_R(Endpoints, peer_endpoints);

    /*-------.
    | Status |
    `-------*/
    public:

    /*-------.
    | States |
    `-------*/
      void
      _publish_interfaces_wrapper();
      void
      _connection_wrapper();
      void
      _wait_for_peer_wrapper();
      void
      _transfer_wrapper();
      void
      _cloud_buffer_wrapper();
      void
      _cloud_synchronize_wrapper();
      void
      _stopped_wrapper();

    protected:
      virtual
      void
      _publish_interfaces() = 0;
      virtual
      void
      _connection() = 0;
      virtual
      void
      _wait_for_peer() = 0;
      virtual
      void
      _transfer() = 0;
      virtual
      void
      _cloud_buffer() = 0;
      virtual
      void
      _stopped() = 0;
      virtual
      void
      _cloud_synchronize() = 0;

      // Invoked right before the fsm is run, each time
      virtual
      void
      _initialize() = 0;

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };
    /* Transferer with a Frete (p2p RPC client/server), a station Host
    * (p2p connection), delegating effective state operations to
    * owner transaction machine.
    */
  }
}

#endif
