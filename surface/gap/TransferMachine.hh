#ifndef SURFACE_GAP_TRANSFER_MACHINE_HH
# define SURFACE_GAP_TRANSFER_MACHINE_HH

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/network/socket.hh>

# include <frete/RPCFrete.hh>

# include <station/fwd.hh>

# include <surface/gap/fwd.hh>

namespace surface
{
  namespace gap
  {
    class Transferer:
      public elle::Printable
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      Transferer(TransactionMachine& owner);

      /*--------.
      | Control |
      `--------*/
    public:
      void
      run();

    protected:
      TransactionMachine& _owner;
      reactor::fsm::Machine _fsm;

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
      virtual
      float
      progress() const;

      virtual
      bool
      finished() const;

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

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };

    class TransferMachine:
      public Transferer
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      TransferMachine(TransactionMachine& owner);
      ELLE_ATTRIBUTE(std::unique_ptr<station::Host>, host);
      ELLE_ATTRIBUTE(std::unique_ptr<infinit::protocol::Serializer>,
                     serializer);
      ELLE_ATTRIBUTE(std::unique_ptr<infinit::protocol::ChanneledStream>,
                     channels);
      ELLE_ATTRIBUTE(std::unique_ptr<frete::RPCFrete>, rpcs);

    private:
      std::unique_ptr<station::Host>
      _connect();

    protected:
      virtual
      void
      _publish_interfaces() override;
      virtual
      void
      _connection() override;
      virtual
      void
      _wait_for_peer() override;
      virtual
      void
      _transfer() override;
      virtual
      void
      _cloud_buffer() override;
      virtual
      void
      _stopped() override;
      virtual
      void
      _cloud_synchronize() override;

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
