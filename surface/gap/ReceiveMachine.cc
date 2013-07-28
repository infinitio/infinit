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
    ReceiveMachine::ReceiveMachine(surface::gap::State const& state):
      TransferMachine(state),
      _wait_for_decision_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_wait_for_decision, this))),
      _accept_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_accept, this))),
      _reject_state(
        this->_machine.state_make(
          std::bind(&ReceiveMachine::_reject, this)))
    {
      // Normal process.
      this->_machine.transition_add(_wait_for_decision_state,
                                    _accept_state,
                                    reactor::Waitables{this->_accepted},
                                    false,
                                    [&] () -> bool
                                    {
                                      return this->_accepted.signaled();
                                    });
      this->_machine.transition_add(_wait_for_decision_state,
                                    _reject_state,
                                    reactor::Waitables{this->_rejected},
                                    false,
                                    [&] () -> bool
                                    {
                                      return this->_rejected.signaled();
                                    });
      this->_machine.transition_add(_accept_state,
                                    _transfer_core_state,
                                    reactor::Waitables{this->_ready},
                                    false,
                                    [&] () -> bool
                                    {
                                      return this->_ready.signaled();
                                    });
      this->_machine.transition_add(_transfer_core_state, _clean_state);

      // Cancel.
      // this->_machine.transition_add(_wait_for_decision_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_accept_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_reject_state, _cancel_state, reactor::Waitables{this->_canceled}, true);
      // this->_machine.transition_add(_transfer_core_state, _cancel_state, reactor::Waitables{this->_canceled}, true);

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
                                   plasma::Transaction const& transaction):
      ReceiveMachine(state)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, transaction);
      this->transaction_id(transaction.id);
      this->network_id(transaction.network_id);
      this->peer_id(transaction.sender_id);

      auto& null = this->_machine.state_make([] {});
      switch (transaction.status)
      {
        case plasma::TransactionStatus::initialized:
          this->run(this->_wait_for_decision_state);
          break;
        case plasma::TransactionStatus::accepted:
          this->_machine.transition_add(null,
                                        this->_transfer_core_state,
                                        reactor::Waitables{this->_ready},
                                        false,
                                        [&] () -> bool
                                        {
                                          return this->_ready.signaled();
                                        });
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
    ReceiveMachine::on_transaction_update(plasma::Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction %s",
                       *this, transaction);

      ELLE_ASSERT_EQ(this->transaction_id(), transaction.id);
      switch (transaction.status)
      {
        case plasma::TransactionStatus::canceled:
          this->_canceled.signal();
          break;
        case plasma::TransactionStatus::failed:
          this->_failed.signal();
          break;
        case plasma::TransactionStatus::finished:
          this->_finished.signal();
          break;
        case plasma::TransactionStatus::ready:
          this->_ready.signal();
          break;
        case plasma::TransactionStatus::accepted:
        case plasma::TransactionStatus::initialized:
        case plasma::TransactionStatus::rejected:
          break;
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    void
    ReceiveMachine::on_peer_connection_update(PeerConnectionUpdateNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: update with new peer connection status %s",
                       *this, notif);

      ELLE_ASSERT_EQ(this->network_id(), notif.network_id);

      if (notif.status)
        this->_peer_online.signal();
      else
        this->_peer_offline.signal();
    }

    void
    ReceiveMachine::accept()
    {
      ELLE_TRACE_SCOPE("%s: accept transaction %s", *this, this->transaction_id());
      this->_accepted.signal();
    }

    void
    ReceiveMachine::reject()
    {
      ELLE_TRACE_SCOPE("%s: reject transaction %s", *this, this->transaction_id());
      this->_rejected.signal();
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision %s", *this, this->transaction_id());
      this->network_id(this->state().meta().transaction(this->transaction_id()).network_id);
    }

    void
    ReceiveMachine::_accept()
    {
      ELLE_TRACE_SCOPE("%s: accepted %s", *this, this->transaction_id());

      this->state().meta().network_add_device(
        this->network_id(), this->state().device_id());

      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::accepted,
                                              this->state().device_id(),
                                              this->state().device_name());
    }

    void
    ReceiveMachine::_reject()
    {
      ELLE_TRACE_SCOPE("%s: rejected %s", *this, this->transaction_id());
      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::rejected);
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

      this->state().meta().update_transaction(
        this->transaction_id(), plasma::TransactionStatus::finished);
    }

    std::string
    ReceiveMachine::type() const
    {
      return "ReceiveMachine";
    }
  }
}
