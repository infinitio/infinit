#ifndef RECEIVEMACHINE_HH
# define RECEIVEMACHINE_HH

# include <surface/gap/State.hh>

# include <reactor/waitable.hh>
# include <reactor/signal.hh>

# include <surface/gap/TransferMachine.hh>

# include <memory>
# include <string>
# include <unordered_set>

namespace surface
{
  namespace gap
  {
    struct ReceiveMachine:
      public TransferMachine
    {

    public:
      ReceiveMachine(surface::gap::State const& state,
                     std::string const& transaction_id);
      virtual
      ~ReceiveMachine();

      virtual
      void
      on_transaction_update(plasma::Transaction const& transaction) override;

      virtual
      void
      on_peer_connection_update(PeerConnectionUpdateNotification const& notif) override;

    public:
      void
      accept();

      void
      rejected();

    private:
      ReceiveMachine(surface::gap::State const& state);

    private:
      void
      _wait_for_decision();

      void
      _accept();

      void
      _reject();

      void
      _transfer_operation() override;

      void
      _clean();

      void
      _fail();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_decision_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, accept_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, reject_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, clean_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, fail_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Signal, accepted);
      ELLE_ATTRIBUTE(reactor::Signal, rejected);
      ELLE_ATTRIBUTE(reactor::Signal, finished);
      ELLE_ATTRIBUTE(reactor::Signal, ready);
      ELLE_ATTRIBUTE(reactor::Signal, canceled);
      ELLE_ATTRIBUTE(reactor::Signal, failed);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);
    };
  }
}

#endif
