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

      virtual
      ~SendMachine();

    public:
      virtual
      void
      on_transaction_update(plasma::Transaction const& transaction) override;

      virtual
      void
      on_peer_connection_update(PeerConnectionUpdateNotification const& notif) override;

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

      void
      _clean();

      void
      _fail();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, request_network_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, create_transaction_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, copy_files_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_accept_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, set_permissions_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, clean_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, fail_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Signal, accepted);
      ELLE_ATTRIBUTE(reactor::Signal, finished);
      ELLE_ATTRIBUTE(reactor::Signal, failed);

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
