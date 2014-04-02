#include <elle/log.hh>

#include <surface/gap/onboarding/ReceiveMachine.hh>
#include <surface/gap/onboarding/TransferMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.onboarding.ReceiveMachine");

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      ReceiveMachine::ReceiveMachine(
        surface::gap::State const& state,
        uint32_t id,
        std::shared_ptr<TransactionMachine::Data> transaction,
        reactor::Duration duration)
        : surface::gap::ReceiveMachine(state, id, transaction)
      {
        ELLE_TRACE_SCOPE("%s: construction", *this);

        this->_transfer_machine.reset(new TransferMachine(*this, duration));
      }

      float
      ReceiveMachine::progress() const
      {
        return this->_transfer_machine->progress();
      }

      void
      ReceiveMachine::accept()
      {
        this->_accepted.open();
      }

      bool
      ReceiveMachine::pause()
      {
        auto* machine = static_cast<TransferMachine*>(
          this->_transfer_machine.get());
        ELLE_TRACE_SCOPE("%s: machine %spaused",
                         *this, (machine->running().opened() ? "" : "un"));
        if (machine->running().opened())
          machine->running().close();
        else
          machine->running().open();
        return machine->running().opened();
      }

      void
      ReceiveMachine::interrupt()
      {
        auto* machine = static_cast<TransferMachine*>(
          this->_transfer_machine.get());
        machine->interrupt();
      }

      void
      ReceiveMachine::_accept()
      {
        this->current_state(TransactionMachine::State::RecipientAccepted);
      }

      void
      ReceiveMachine::_finalize(infinit::oracles::Transaction::Status)
      {
      }
    }
  }
}
