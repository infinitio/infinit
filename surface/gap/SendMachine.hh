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
      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::string const& recipient,
                  std::unordered_set<std::string>&& files,
                  std::string const& message,
                  std::shared_ptr<Data> data);

      SendMachine(surface::gap::State const& state,
                  uint32_t id,
                  std::shared_ptr<Data> data);

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

    private:
      void
      _request_network();

      void
      _create_transaction();

      void
      _copy_files();

      void
      _wait_for_accept();

      void
      _set_permissions();

      void
      _transfer_operation() override;

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, request_network_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, copy_files_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, set_permissions_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Barrier, accepted);
      ELLE_ATTRIBUTE(reactor::Barrier, rejected);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);
      ELLE_ATTRIBUTE(std::string, message);

    public:
      virtual
      bool
      is_sender() const override
      {
        return true;
      }

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
