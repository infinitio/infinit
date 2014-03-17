#ifndef SURFACE_GAP_TRANSFER_MACHINE_HH
# define SURFACE_GAP_TRANSFER_MACHINE_HH

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/network/socket.hh>

# include <frete/RPCFrete.hh>
# include <surface/gap/fwd.hh>

namespace surface
{
  namespace gap
  {
    class TransferMachine:
      public elle::Printable
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      TransferMachine(TransactionMachine& owner);
      ELLE_ATTRIBUTE(TransactionMachine&, owner);
      ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::Socket>, host);
      ELLE_ATTRIBUTE(std::unique_ptr<infinit::protocol::Serializer>,
                     serializer);
      ELLE_ATTRIBUTE(std::unique_ptr<infinit::protocol::ChanneledStream>,
                     channels);
      ELLE_ATTRIBUTE(std::unique_ptr<frete::RPCFrete>, rpcs);

    /*--------.
    | Control |
    `--------*/
    public:
      void
      run();
    private:
      ELLE_ATTRIBUTE(reactor::fsm::Machine, fsm);

    /*---------.
    | Triggers |
    `---------*/
    private:
      // Represents the connection status of the peer according to the servers.
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_online);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_offline);
      // Represents the availability of the peer for peer to peer connection.
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_reachable);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_unreachable);
      // Signal that the peer is connected to us.
      ELLE_ATTRIBUTE(reactor::Signal,  peer_connected);

    /*-------.
    | Status |
    `-------*/
    public:
      float
      progress() const;
      bool
      finished() const;

    /*-------.
    | States |
    `-------*/
    private:
      void
      _publish_interfaces();
      std::unique_ptr<reactor::network::Socket>
      _connect();
      void
      _connection();
      void
      _wait_for_peer();
      void
      _transfer();
      void
      _stopped();

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;

    //   // Common on both sender and recipient process.
    //   ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
    //   ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
    //   // XXX: I you disconnect, which is quasi transparent, you must publish your interfaces.
    //   ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_peer_state);
    //   ELLE_ATTRIBUTE(reactor::fsm::State&, transfer_state);
    //   ELLE_ATTRIBUTE(reactor::fsm::State&, core_stoped_state);
    //   ELLE_ATTRIBUTE(reactor::fsm::State&, core_paused_state);

    };
  }
}

#endif
