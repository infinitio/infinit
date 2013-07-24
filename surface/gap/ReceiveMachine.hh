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
      on_user_update(plasma::meta::User const& user) override;

      virtual
      void
      on_network_update(plasma::meta::NetworkResponse const& network) override;

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
      _publish_interfaces();

      void
      _connection();

      void
      _transfer();

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
      // Common on both sender and recipient process, could be put in base class.
      ELLE_ATTRIBUTE(reactor::fsm::State&, publish_interfaces_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, connection_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, transfer_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, clean_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, fail_state);

      // User status signal.
      ELLE_ATTRIBUTE(reactor::Signal, peer_online);
      ELLE_ATTRIBUTE(reactor::Signal, peer_offline);

      // Slug signal.
      ELLE_ATTRIBUTE(reactor::Signal, peer_connected);
      ELLE_ATTRIBUTE(reactor::Signal, peer_disconnected);

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
