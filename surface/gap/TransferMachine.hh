#ifndef SURFACE_GAP_TRANSFER_MACHINE_HH
# define SURFACE_GAP_TRANSFER_MACHINE_HH

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/network/socket.hh>

# include <frete/Frete.hh>
# include <surface/gap/fwd.hh>

namespace surface
{
  namespace gap
  {
    class TransferMachine
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
      ELLE_ATTRIBUTE(std::unique_ptr<frete::Frete>, frete);


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
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_online);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_offline);
      ELLE_ATTRIBUTE(reactor::Signal,  peer_connected);

    /*-------.
    | Status |
    `-------*/
    public:
      float
      progress() const;
      ELLE_ATTRIBUTE(bool, finished);

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
