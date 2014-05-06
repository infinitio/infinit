#ifndef SENDMACHINE_HH
# define SENDMACHINE_HH

# include <surface/gap/TransactionMachine.hh>
# include <surface/gap/State.hh>

# include <frete/fwd.hh>

namespace surface
{
  namespace gap
  {
    class  SendMachine:
      public TransactionMachine
    {
    public:
      // Construct from send files.
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::string const& recipient,
                  std::unordered_set<std::string>&& files,
                  std::string const& message,
                  std::shared_ptr<Data> data);

      // Construct from snapshot.
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::unordered_set<std::string> files,
                  TransactionMachine::State current_state,
                  std::string const& message,
                  std::shared_ptr<TransactionMachine::Data> data);

      // Construct from server data.
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::shared_ptr<TransactionMachine::Data> data);


      virtual
      ~SendMachine();

    public:
      virtual
      void
      transaction_status_update(infinit::oracles::Transaction::Status status) override;

    private:
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::shared_ptr<Data> data,
                  bool);

      OldSnapshot
      _make_snapshot() const override;

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
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE_RX(reactor::Barrier, accepted);
      ELLE_ATTRIBUTE_RX(reactor::Barrier, rejected);

      ELLE_ATTRIBUTE(std::unique_ptr<frete::Frete>, frete);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      /// List of path from transaction data.
      typedef std::unordered_set<std::string> Files;
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
