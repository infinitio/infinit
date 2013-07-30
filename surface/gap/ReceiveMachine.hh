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
                     plasma::Transaction const& transaction);

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
      reject();

    private:
      ReceiveMachine(surface::gap::State const& state);

    private:
      void
      _wait_for_decision();

      void
      _accept();

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

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Barrier, accepted);
      ELLE_ATTRIBUTE(reactor::Barrier, ready);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
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
