#ifndef SURFACE_GAP_PEER_MACHINE_HH
# define SURFACE_GAP_PEER_MACHINE_HH

# include <surface/gap/TransactionMachine.hh>

namespace surface
{
  namespace gap
  {
    class PeerMachine:
      virtual public TransactionMachine
    {
    /*-----.
    | Type |
    `-----*/
    public:
      typedef PeerMachine Self;
      typedef TransactionMachine Super;
      typedef infinit::oracles::PeerTransaction Data;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      PeerMachine(Transaction& transaction,
                  uint32_t id,
                  std::shared_ptr<Data> data);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);

    /*---------.
    | Transfer |
    `---------*/
    public:
      virtual
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& socket) = 0;
      virtual
      float
      progress() const override;
    protected:
      friend class Transferer;
      friend class PeerTransferMachine;
      virtual
      void
      _transfer_operation(frete::RPCFrete& frete) = 0;
      virtual
      // Go all the way to the cloud until interrupted. Only throws reactor::Terminate
      void
      _cloud_operation() = 0;
      virtual
      // Just synchronize what you can with cloud
      void
      _cloud_synchronize() = 0;
      std::unique_ptr<Transferer> _transfer_machine;
      virtual
      void
      _transfer() override;
      virtual
      void
      peer_available(std::vector<std::pair<std::string, int>> const& local_endpoints,
                     std::vector<std::pair<std::string, int>> const& public_endpoints
        ) override;
      virtual
      void
      peer_unavailable() override;
      void
      _peer_connection_changed(bool user_status);
      virtual
      std::unique_ptr<infinit::oracles::meta::CloudCredentials>
      _cloud_credentials(bool regenerate) override;
      virtual
      void
      _finalize(infinit::oracles::Transaction::Status) override;
    };
  }
}

#endif
