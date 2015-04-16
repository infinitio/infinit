#ifndef SURFACE_GAP_TRANSFER_MACHINE_HH
# define SURFACE_GAP_TRANSFER_MACHINE_HH

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/timer.hh>

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
      virtual
      ~Transferer() noexcept(true) {};

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
    public:
      /// Notify the peer is available for peer to peer connection.
      void
      peer_available(std::vector<std::pair<std::string, int>> const& local_endpoints,
                     std::vector<std::pair<std::string, int>> const& public_endpoints);
      /// Notify the peer is unavailable for peer to peer connection.
      void
      peer_unavailable();

    private:
      // Connection status of the peer according to the servers.
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_online);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, peer_offline);
      // Availability of the peer for peer to peer connection.
      ELLE_ATTRIBUTE(reactor::Barrier, peer_reachable);
      ELLE_ATTRIBUTE(reactor::Barrier, peer_unreachable);
      // The peer endpoints.
      typedef std::vector<std::pair<std::string, int>> Endpoints;
      ELLE_ATTRIBUTE_R(Endpoints, peer_local_endpoints);
      ELLE_ATTRIBUTE_R(Endpoints, peer_public_endpoints);
      // Number of connection attempts so far
      ELLE_ATTRIBUTE_RP(int, attempt, protected:);
      // Timer for delayed gap transaction transitionning to connecting
      ELLE_ATTRIBUTE_R(std::unique_ptr<reactor::Timer>, gap_connecting_delay);
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
  }
}

#endif
