#ifndef SURFACE_GAP_GHOST_RECIPIENT_TRANSFER_MACHINE_HH
# define SURFACE_GAP_GHOST_RECIPIENT_TRANSFER_MACHINE_HH

# include <reactor/Barrier.hh>
# include <reactor/fsm.hh>
# include <reactor/network/socket.hh>

# include <surface/gap/TransferMachine.hh>



namespace surface
{
  namespace gap
  {
    /// Transfer machine fetching data from a dl-by-link upload
    class GhostRecipientTransferMachine:
      public BaseTransferer
    {
    public:
      GhostRecipientTransferMachine(TransactionMachine& owner);
      virtual void run() override;
      virtual void print(std::ostream&) const override;
      virtual void peer_available(Endpoints const& endpoints) override;
      virtual void peer_unavailable() override;
    };
  }
}
#endif
