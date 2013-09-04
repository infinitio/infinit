#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/Rounds.hh>
#include <surface/gap/_detail/TransferOperations.hh>

#include <nucleus/neutron/Subject.hh>

#include <reactor/thread.hh>
#include <reactor/exception.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine");

namespace surface
{
  namespace gap
  {
    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransferMachine::Data> data,
                                   bool):
      TransferMachine(state, id, std::move(data)),
      _wait_for_decision_state(
        this->_machine.state_make(
          "wait for decision", std::bind(&ReceiveMachine::_wait_for_decision, this))),
      _accept_state(
        this->_machine.state_make(
          "accept", std::bind(&ReceiveMachine::_accept, this))),
      _wait_for_ready_state(
        this->_machine.state_make(
          "wait for ready", std::bind(&ReceiveMachine::_wait_for_ready, this)))

    {
      // Normal way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_accept_state,
                                    reactor::Waitables{&this->_accepted});
      this->_machine.transition_add(this->_accept_state,
                                    this->_wait_for_ready_state);
      this->_machine.transition_add(this->_wait_for_ready_state,
                                    this->_transfer_core_state,
                                    reactor::Waitables{&this->_ready});
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_finish_state);

      // Reject way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_reject_state,
                                    reactor::Waitables{&this->_rejected});

      // Cancel.
      this->_machine.transition_add(_wait_for_decision_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(_wait_for_ready_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(_accept_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(_reject_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(_transfer_core_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);

      // Exception.
      this->_machine.transition_add_catch(_wait_for_decision_state, _fail_state);
      this->_machine.transition_add_catch(_wait_for_ready_state, _fail_state);
      this->_machine.transition_add_catch(_accept_state, _fail_state);
      this->_machine.transition_add_catch(_reject_state, _fail_state);
      this->_machine.transition_add_catch(_transfer_core_state, _fail_state);
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   TransferMachine::State const current_state,
                                   std::shared_ptr<TransferMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s, starting at %s",
                       *this, *this->data(), current_state);

      switch (current_state)
      {
        case TransferMachine::State::NewTransaction:
          //
          break;
        case TransferMachine::State::SenderCreateNetwork:
        case TransferMachine::State::SenderCreateTransaction:
        case TransferMachine::State::SenderCopyFiles:
        case TransferMachine::State::SenderWaitForDecision:
          elle::unreachable();
        case TransferMachine::State::RecipientWaitForDecision:
          this->_run(this->_wait_for_decision_state);
          break;
        case TransferMachine::State::RecipientAccepted:
          this->_run(this->_accept_state);
          break;
        case TransferMachine::State::RecipientWaitForReady:
          this->_run(this->_wait_for_ready_state);
          break;
        case TransferMachine::State::GrantPermissions:
          elle::unreachable();
        case TransferMachine::State::PublishInterfaces:
        case TransferMachine::State::Connect:
        case TransferMachine::State::PeerDisconnected:
        case TransferMachine::State::PeerConnectionLost:
        case TransferMachine::State::Transfer:
          this->_run(this->_transfer_core_state);
          break;
        case TransferMachine::State::CleanLocal:
          this->_run(this->_local_clean_state);
          break;
        case TransferMachine::State::CleanRemote:
          this->_run(this->_remote_clean_state);
          break;
        case TransferMachine::State::Finished:
          this->_run(this->_finish_state);
          break;
        case TransferMachine::State::Rejected:
          this->_run(this->_reject_state);
          break;
        case TransferMachine::State::Canceled:
          this->_run(this->_cancel_state);
          break;
        case TransferMachine::State::Failed:
          this->_run(this->_fail_state);
          break;
        default:
          elle::unreachable();
      }
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransferMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, data);

      switch (this->data()->status)
      {
        case plasma::TransactionStatus::initialized:
          this->_run(this->_wait_for_decision_state);
          break;
        case plasma::TransactionStatus::accepted:
          this->_run(this->_wait_for_ready_state);
          break;
        case plasma::TransactionStatus::ready:
          this->_run(this->_transfer_core_state);
          break;
        case plasma::TransactionStatus::finished:
          this->_run(this->_finish_state);
          break;
        case plasma::TransactionStatus::canceled:
          this->_run(this->_cancel_state);
          break;
        case plasma::TransactionStatus::failed:
          this->_run(this->_fail_state);
          break;
        case plasma::TransactionStatus::rejected:
        case plasma::TransactionStatus::created:
          break;
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    ReceiveMachine::~ReceiveMachine()
    {
      this->_stop();
    }

    void
    ReceiveMachine::transaction_status_update(plasma::TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
        case plasma::TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this)
            this->_canceled.open();
          break;
        case plasma::TransactionStatus::failed:
          ELLE_DEBUG("%s: open failed barrier", *this)
            this->_failed.open();
          break;
        case plasma::TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->_finished.open();
          break;
        case plasma::TransactionStatus::ready:
          ELLE_DEBUG("%s: open ready barrier", *this)
            this->_ready.open();
          break;
        case plasma::TransactionStatus::accepted:
        case plasma::TransactionStatus::rejected:
        case plasma::TransactionStatus::initialized:
          ELLE_DEBUG("%s: ignore status %s", *this, status);
          break;
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    void
    ReceiveMachine::accept()
    {
      ELLE_TRACE_SCOPE("%s: open accept barrier %s", *this, this->transaction_id());

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      this->_accepted.open();
    }

    void
    ReceiveMachine::reject()
    {
      ELLE_TRACE_SCOPE("%s: open rejected barrier %s", *this, this->transaction_id());
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      this->_rejected.open();
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision %s", *this, this->transaction_id());
      this->current_state(TransferMachine::State::RecipientWaitForDecision);
    }

    void
    ReceiveMachine::_accept()
    {
      ELLE_TRACE_SCOPE("%s: accepted %s", *this, this->transaction_id());
      this->current_state(TransferMachine::State::RecipientAccepted);

      ELLE_TRACE("%s: add device %s to network %s",
                 *this, this->state().device().id, this->network_id());
      this->state().meta().network_add_device(
        this->network_id(), this->state().device().id);

      try
      {
        this->state().meta().update_transaction(this->transaction_id(),
                                                plasma::TransactionStatus::accepted,
                                                this->state().device().id,
                                                this->state().device().name);
      }
      catch (plasma::meta::Exception const& e)
      {
        if (e.err == plasma::meta::Error::transaction_already_has_this_status)
          ELLE_TRACE("%s: transaction already accepted: %s", *this, e.what());
        else if (e.err == plasma::meta::Error::transaction_operation_not_permitted)
          ELLE_TRACE("%s: transaction can't be accepted: %s", *this, e.what());
        else
          throw;
      }
    }

    void
    ReceiveMachine::_wait_for_ready()
    {
      ELLE_TRACE_SCOPE("%s: wait for ready %s", *this, this->transaction_id());
      this->current_state(TransferMachine::State::RecipientWaitForReady);
    }

    void
    ReceiveMachine::_transfer_operation()
    {
      ELLE_TRACE_SCOPE("%s: transfer operation %s", *this, this->transaction_id());

      nucleus::neutron::Subject subject;
      subject.Create(this->state().identity().pair().K());

      operation_detail::from::receive(this->etoile(),
                                      this->descriptor(),
                                      subject,
                                      this->state().output_dir());

      this->_finished.open();
    }

    std::string
    ReceiveMachine::type() const
    {
      return "ReceiveMachine";
    }
  }
}
