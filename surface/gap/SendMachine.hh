#ifndef SURFACE_GAP_SEND_MACHINE_HH
# define SURFACE_GAP_SEND_MACHINE_HH

# include <surface/gap/PeerTransactionMachine.hh>
# include <surface/gap/State.hh>

# include <frete/fwd.hh>

namespace surface
{
  namespace gap
  {
    class  SendMachine:
      public PeerTransactionMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef SendMachine Self;
      typedef PeerTransactionMachine Super;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Construct to send files.
      SendMachine(Transaction& transaction,
                  uint32_t id,
                  std::string const& recipient,
                  std::vector<std::string> files,
                  std::string const& message,
                  std::shared_ptr<Data> data);
      /// Construct from snapshot.
      SendMachine(Transaction& transaction,
                  uint32_t id,
                  std::vector<std::string> files,
                  std::string const& message,
                  std::shared_ptr<Data> data);
      virtual
      ~SendMachine();
    private:
      void
      _run_from_snapshot();

    public:
      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) override;

    private:
      /// Factored constructor.
      SendMachine(Transaction& transaction,
                  uint32_t id,
                  std::shared_ptr<Data> data);

    private:
      void
      _create_transaction();
      void
      _wait_for_accept();
      void
      _transfer_operation(frete::RPCFrete& frete) override;
      // chunked upload to cloud
      void
      _cloud_operation() override;
      void
      _cloud_synchronize() override;
      // cleartext upload one file to cloud
      void
      _ghost_cloud_upload();
      bool
      _fetch_peer_key(bool assert_success);

    /*-----------------------.
    | Machine implementation |
    `-----------------------*/
    public:
      virtual
      void
      notify_user_connection_status(std::string const& user_id,
                                    std::string const& device_id,
                                    bool online) override;
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);
      // Transaction status signals.
      ELLE_ATTRIBUTE_RX(reactor::Barrier, accepted);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, rejected);
      ELLE_ATTRIBUTE(std::unique_ptr<frete::Frete>, frete);

    /*-----------------.
    | Transaction data |
    `-----------------*/
    public:
      typedef std::vector<std::string> Files;
      ELLE_ATTRIBUTE(Files, files);
      ELLE_ATTRIBUTE(std::string, message);
    protected:
      void
      _save_transfer_snapshot() override;

    public:
      float
      progress() const override;

    public:
      virtual
      bool
      is_sender() const override
      {
        return true;
      }

      frete::Frete&
      frete();

    private:
      std::unique_ptr<frete::RPCFrete>
      rpcs(infinit::protocol::ChanneledStream& channels) override;
    /*----------.
    | Printable |
    `----------*/
    protected:
      virtual
      void cleanup () override;
    };
  }
}

#endif
