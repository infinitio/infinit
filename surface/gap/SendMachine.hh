#ifndef SENDMACHINE_HH
# define SENDMACHINE_HH

# include <surface/gap/TransferMachine.hh>
# include <surface/gap/State.hh>

namespace surface
{
  namespace gap
  {
    class  SendMachine:
      public TransferMachine
    {
    public:
      // Construct from send files.
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::string const& recipient,
                  std::unordered_set<std::string>&& files,
                  std::string const& message,
                  std::shared_ptr<Data> data);

      // Construct from snapshot (with curren_state and files).
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::unordered_set<std::string> files,
                  TransferMachine::State current_state,
                  std::string const& message,
                  std::shared_ptr<TransferMachine::Data> data);

      // XXX: Add putain de commentaire de la vie.
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::shared_ptr<TransferMachine::Data> data);


      virtual
      ~SendMachine();

    public:
      virtual
      void
      transaction_status_update(plasma::TransactionStatus status) override;

    private:
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::shared_ptr<Data> data,
                  bool);

      Snapshot
      _make_snapshot() const override;

    private:
      void
      _create_transaction();

      void
      _wait_for_accept();

      void
      _transfer_operation() override;

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Barrier, accepted);
      ELLE_ATTRIBUTE(reactor::Barrier, rejected);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      typedef std::unordered_set<std::string> Files;
      ELLE_ATTRIBUTE(Files, files);
      ELLE_ATTRIBUTE(std::string, message);

    public:
      virtual
      bool
      is_sender() const override
      {
        return true;
      }

    private:
      frete::Frete&
      frete() override;

      std::pair<uint32_t, std::ifstream> _current_file;

    public:
      /*----------.
      | Printable |
      `----------*/
      std::string
      type() const override;
    };
  }
}

#endif
