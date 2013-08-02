#ifndef SENDMACHINE_HH
# define SENDMACHINE_HH

# include "usings.hh"

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
                  std::string const& recipient,
                  std::unordered_set<std::string>&& files);

      SendMachine(surface::gap::State const& state,
                  plasma::Transaction const& transaction);

      virtual
      ~SendMachine();

    public:
      virtual
      void
      on_transaction_update(plasma::Transaction const& transaction) override;

    private:
      SendMachine(surface::gap::State const& state);

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
