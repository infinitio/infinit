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
      // Construct from notification.
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     std::shared_ptr<TransferMachine::Data> data);

      // Construct from snapshot (with current_state).
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     TransferMachine::State const current_state,
                     std::shared_ptr<TransferMachine::Data> data);
      virtual
      ~ReceiveMachine();

      virtual
      void
      transaction_status_update(plasma::TransactionStatus status) override;

    public:
      void
      accept();

      void
      reject();

    private:
      ReceiveMachine(surface::gap::State const& state,
                     uint32_t id,
                     std::shared_ptr<TransferMachine::Data> data,
                     bool);

    private:
      void
      _wait_for_decision();

      void
      _accept();

      void
      _transfer_operation() override;

      void
      _fail();

      /*-----------------------.
      | Machine implementation |
      `-----------------------*/
      ELLE_ATTRIBUTE(reactor::fsm::State&, wait_for_decision_state);
      ELLE_ATTRIBUTE(reactor::fsm::State&, accept_state);

      // Transaction status signals.
      ELLE_ATTRIBUTE(reactor::Barrier, accepted);

      /*-----------------.
      | Transaction data |
      `-----------------*/
      ELLE_ATTRIBUTE(std::string, recipient);
      ELLE_ATTRIBUTE(std::unordered_set<std::string>, files);

    public:
      virtual
      bool
      is_sender() const override
      {
        return false;
      }

    private:
      virtual
      void
      _enable_rpcs() override;

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
