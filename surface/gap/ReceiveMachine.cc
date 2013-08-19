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
          "accept", std::bind(&ReceiveMachine::_accept, this)))
    {
      // Normal way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_accept_state,
                                    reactor::Waitables{&this->_accepted});
      this->_machine.transition_add(this->_accept_state,
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
      this->_machine.transition_add(_accept_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(_reject_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(_transfer_core_state, _cancel_state, reactor::Waitables{&this->_canceled}, true);

      // Exception.
      this->_machine.transition_add_catch(_wait_for_decision_state, _fail_state);
      this->_machine.transition_add_catch(_accept_state, _fail_state);
      this->_machine.transition_add_catch(_reject_state, _fail_state);
      this->_machine.transition_add_catch(_transfer_core_state, _fail_state);
    }

    ReceiveMachine::~ReceiveMachine()
    {
      this->_stop();
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransferMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, data);

      auto& null = this->_machine.state_make([] {});
      switch (this->data()->status)
      {
        case plasma::TransactionStatus::initialized:
          this->run(this->_wait_for_decision_state);
          break;
        case plasma::TransactionStatus::accepted:
          this->_machine.transition_add(null,
                                        this->_transfer_core_state,
                                        reactor::Waitables{&this->_ready});
          this->run(null);
          break;
        case plasma::TransactionStatus::ready:
          this->run(this->_transfer_core_state);
          break;
        case plasma::TransactionStatus::finished:
          this->run(this->_finish_state);
          break;
        case plasma::TransactionStatus::canceled:
          this->run(this->_cancel_state);
          break;
        case plasma::TransactionStatus::failed:
          this->run(this->_fail_state);
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

    void
    ReceiveMachine::transaction_status_update(plasma::TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      auto& scheduler = *reactor::Scheduler::scheduler();

      switch (status)
      {
        case plasma::TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this);
          scheduler.mt_run<void>("open canceled barrier", [this]
            {
              this->_canceled.open();
            });
          break;
        case plasma::TransactionStatus::failed:
          ELLE_DEBUG("%s: open failed barrier", *this);
          scheduler.mt_run<void>("open failed barrier", [this]
            {
              this->_failed.open();
            });
          break;
        case plasma::TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this);
          scheduler.mt_run<void>("open finished barrier", [this]
            {
              this->_finished.open();
            });
          break;
        case plasma::TransactionStatus::ready:
          ELLE_DEBUG("%s: open ready barrier", *this);
          scheduler.mt_run<void>("open ready barrier", [this]
            {
              this->_ready.open();
            });
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

      auto& scheduler = *reactor::Scheduler::scheduler();

      scheduler.mt_run<void>("open accept barrier", [this]
        {
          this->_accepted.open();
        });
    }

    void
    ReceiveMachine::reject()
    {
      ELLE_TRACE_SCOPE("%s: open rejected barrier %s", *this, this->transaction_id());
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      auto& scheduler = *reactor::Scheduler::scheduler();

      scheduler.mt_run<void>("open reject barrier", [this]
        {
          this->_rejected.open();
        });
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision %s", *this, this->transaction_id());
      this->state().enqueue(Notification(this->id(), TransferState_RecipientWaitForDecision));

      auto network_id =
        this->state().meta().transaction(this->transaction_id()).network_id;
      this->network_id(network_id);
    }

    void
    ReceiveMachine::_accept()
    {
      ELLE_TRACE_SCOPE("%s: accepted %s", *this, this->transaction_id());
      this->state().enqueue(Notification(this->id(), TransferState_RecipientAccepted));

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
    ReceiveMachine::_transfer_operation()
    {
      ELLE_TRACE_SCOPE("%s: transfer operation %s", *this, this->transaction_id());

      nucleus::neutron::Subject subject;
      subject.Create(this->state().identity().pair().K());

      operation_detail::from::receive(this->etoile(),
                                      this->descriptor(),
                                      subject,
                                      this->state().output_dir());
    }

    std::string
    ReceiveMachine::type() const
    {
      return "ReceiveMachine";
    }
  }
}
