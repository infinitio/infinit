#ifndef SURFACE_GAP_PEER_SEND_MACHINE_HH
# define SURFACE_GAP_PEER_SEND_MACHINE_HH

# include <memory>

# include <surface/gap/SendMachine.hh>
# include <surface/gap/PeerMachine.hh>
# include <surface/gap/fwd.hh>

namespace surface
{
  namespace gap
  {
    class PeerSendMachine
      : public SendMachine
      , public PeerMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef PeerMachine::Data Data;
      typedef PeerSendMachine Self;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Construct to send files.
      PeerSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::string recipient,
                      std::vector<std::string> files,
                      std::string message,
                      std::shared_ptr<Data> data);
      /// Construct from snapshot.
      PeerSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::vector<std::string> files,
                      std::string message,
                      std::shared_ptr<Data> data);
      /// Constructor when sending from another device or if you have no
      /// snapshot as sender. In that case, run_to_fail is set to true.
      PeerSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::shared_ptr<Data> data,
                      bool run_to_fail = false);
      virtual
      ~PeerSendMachine();
      using PeerMachine::data;
      ELLE_ATTRIBUTE_R(std::string, message);
      ELLE_ATTRIBUTE_R(std::string, recipient);
    private:
      PeerSendMachine(Transaction& transaction,
                      uint32_t id,
                      std::string recipient,
                      std::vector<std::string> files,
                      std::string message,
                      std::shared_ptr<Data> data,
                      bool);
      void
      _run_from_snapshot();

    /*-----------.
    | Attributes |
    `-----------*/
    public:
      ELLE_ATTRIBUTE_RX(reactor::Barrier, accepted);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, rejected);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, ghost_uploaded);
      ELLE_ATTRIBUTE(std::unique_ptr<frete::Frete>, frete);

    /*-------.
    | States |
    `-------*/
    protected:
      void
      _wait_for_accept();
      reactor::fsm::State& _wait_for_accept_state;

    /*---------------.
    | Implementation |
    `---------------*/
    private:
      virtual
      void
      _finish() override;

    public:
      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) override;
      float
      progress() const override;
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& channels) override;
      frete::Frete&
      frete();
    protected:
      virtual
      void
      _create_transaction() override;
      virtual
      void _initialize_transaction() override;
      void
      _transfer_operation(frete::RPCFrete& frete) override;
      // chunked upload to cloud
      void
      _cloud_operation() override;
      void
      _cloud_synchronize() override;
      bool
      _fetch_peer_key(bool assert_success);
      void
      _save_frete_snapshot();
      virtual
      void
      notify_user_connection_status(std::string const& user_id,
                                    bool user_status,
                                    std::string const& device_id,
                                    bool device_status) override;
    };
  }
}

#endif
