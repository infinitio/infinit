#ifndef SURFACE_GAP_PEER_TRANSFER_MACHINE_HH
# define SURFACE_GAP_PEER_TRANSFER_MACHINE_HH

# include <reactor/network/upnp.hh>

# include <frete/RPCFrete.hh>
# include <station/fwd.hh>
# include <surface/gap/PeerMachine.hh>
# include <surface/gap/TransferMachine.hh>
# include <station/Station.hh>

namespace surface
{
  namespace gap
  {
    class PeerTransferMachine:
      public Transferer
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      PeerTransferMachine(PeerMachine& owner);
      virtual
      ~PeerTransferMachine() noexcept(true);

      ELLE_ATTRIBUTE(std::unique_ptr<infinit::protocol::Serializer>,
                     serializer);
      ELLE_ATTRIBUTE(std::unique_ptr<infinit::protocol::ChanneledStream>,
                     channels);
      ELLE_ATTRIBUTE(std::unique_ptr<frete::RPCFrete>, rpcs);
      ELLE_ATTRIBUTE(PeerMachine&, owner);

    private:
      ELLE_ATTRIBUTE(station::Station, station);
      ELLE_ATTRIBUTE(std::shared_ptr<reactor::network::UPNP>, upnp);
      ELLE_ATTRIBUTE(reactor::network::PortMapping, upnp_mapping);
      ELLE_ATTRIBUTE(reactor::Thread, upnp_init_thread);
      void
      _upnp_init();
      std::unique_ptr<station::Host>
      _connect();

      ELLE_ATTRIBUTE(std::unique_ptr<station::Host>, host); /*stay after station!*/

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
      virtual
      void
      _initialize() override;

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
