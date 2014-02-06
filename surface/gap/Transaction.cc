#include <surface/gap/Transaction.hh>
#include <surface/gap/enums.hh>

#include <surface/gap/TransferMachine.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/Exception.hh>

ELLE_LOG_COMPONENT("surface.gap.Transaction");

namespace surface
{
  namespace gap
  {
    Notification::Type Transaction::Notification::type = NotificationType_TransactionUpdate;
    Transaction::Notification::Notification(uint32_t id,
                                            gap_TransactionStatus status):
      id(id),
      status(status)
    {}

    static
    gap_TransactionStatus
    _transaction_status(Transaction::Data const& data,
                        TransferMachine::State state)
    {
      switch (data.status)
      {
        case infinit::oracles::Transaction::Status::finished:
          return gap_transaction_finished;
        case infinit::oracles::Transaction::Status::rejected:
          return gap_transaction_rejected;
        case infinit::oracles::Transaction::Status::failed:
          return gap_transaction_failed;
        case infinit::oracles::Transaction::Status::canceled:
          return gap_transaction_canceled;
        default:
          switch (state)
          {
            case TransferMachine::State::NewTransaction:
            case TransferMachine::State::SenderCreateTransaction:
              // The sender is pending creating the transaction.
              return gap_transaction_pending;
            case TransferMachine::State::SenderWaitForDecision:
              return gap_transaction_waiting_for_accept;
            case TransferMachine::State::RecipientWaitForDecision:
              // The recipient is pending creating waiting for decision.
              return gap_transaction_waiting_for_accept;
            case TransferMachine::State::RecipientAccepted:
              return gap_transaction_accepted;
            case TransferMachine::State::PublishInterfaces:
            case TransferMachine::State::Connect:
            case TransferMachine::State::PeerDisconnected:
            case TransferMachine::State::PeerConnectionLost:
              return gap_transaction_preparing;
            case TransferMachine::State::Transfer:
              return gap_transaction_running;
            case TransferMachine::State::Over:
              return gap_transaction_cleaning;
            case TransferMachine::State::Finished:
              return gap_transaction_finished;
            case TransferMachine::State::Rejected:
              return gap_transaction_rejected;
            case TransferMachine::State::Canceled:
              return gap_transaction_canceled;
            case TransferMachine::State::Failed:
              return gap_transaction_failed;
            case TransferMachine::State::None:
              return gap_transaction_none;
          }
      }
      throw Exception(gap_internal_error,
                      "no transaction status can be deduced");
    }

    // - Exception -------------------------------------------------------------
    Transaction::BadOperation::BadOperation(Type type):
      Exception(gap_error, elle::sprintf("%s", type)),
      _type(type)
    {}

    Transaction::Transaction(State const& state,
                             uint32_t id,
                             Data&& data,
                             bool history):
      _id(id),
      _data(new Data{std::move(data)}),
      _machine(),
      _last_status(gap_transaction_none)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s", *this, *this->_data);
      ELLE_ASSERT(state.me().id == this->_data->sender_id ||
                  state.me().id == this->_data->recipient_id);
      if (history)
      {
        ELLE_DEBUG("%s: history", *this);
        return;
      }

      if (state.me().id == this->_data->sender_id &&
          state.device().id == this->_data->sender_device_id)
      {
        ELLE_TRACE("%s: create send machine from data: %s", *this, *this->_data);
        this->_machine.reset(new SendMachine{state, this->_id, this->_data});
      }
      else if (state.me().id == this->_data->recipient_id &&
               (this->_data->recipient_device_id.empty() ||
                state.device().id == this->_data->recipient_device_id))
      {
        ELLE_TRACE("%s: create receive machine from data: %s", *this, *this->_data);
        this->_machine.reset(new ReceiveMachine{state, this->_id, this->_data});
      }
      else
      {
        ELLE_WARN("%s: no machine can be launch for data %s cause it's not "\
                  "your device (%s)", *this, this->_data, state.device().id);
      }

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      this->_machine_state_thread.reset(
        new reactor::Thread{
          *reactor::Scheduler::scheduler(),
          "notify fsm update",
          [this, &state] ()
          {
            this->_notify_on_status_update(state);
          }});
    }

    Transaction::Transaction(State const& state,
                             uint32_t id,
                             TransferMachine::Snapshot snapshot):
      _id(id),
      _data(new Data{std::move(snapshot.data)}),
      _machine(),
      _last_status(gap_transaction_none)
    {
      ELLE_TRACE_SCOPE("%s: construct from snapshot (%s) with data %s",
                       *this, snapshot.state, *this->_data);

      if (state.me().id == this->_data->sender_id &&
          state.device().id == this->_data->sender_device_id)
      {
        ELLE_TRACE("%s: create send machine from data: %s", *this, *this->_data);
        this->_machine.reset(new SendMachine{state, this->_id, snapshot.files, snapshot.state, snapshot.message, this->_data});
      }
      else if (state.me().id == this->_data->recipient_id &&
               (this->_data->recipient_device_id.empty() ||
                state.device().id == this->_data->recipient_device_id))
      {
        ELLE_TRACE("%s: create receive machine from data: %s", *this, *this->_data);
        this->_machine.reset(new ReceiveMachine{state, this->_id, snapshot.state, this->_data});
      }
      else
      {
        throw Exception(gap_internal_error, "invalid snapshot");
      }

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      this->_machine_state_thread.reset(
        new reactor::Thread{
          *reactor::Scheduler::scheduler(),
          "notify fsm update",
          [this, &state]
          {
            this->_notify_on_status_update(state);
          }});
    }

    Transaction::Transaction(surface::gap::State const& state,
                             uint32_t id,
                             std::string const& peer_id,
                             std::unordered_set<std::string>&& files,
                             std::string const& message):
      _id(id),
      _data(new Data{state.me().id, state.me().fullname, state.device().id}),
      _machine(new SendMachine{state, this->_id, peer_id, std::move(files), message, this->_data}),
      _last_status(gap_transaction_none)
    {
      ELLE_TRACE_SCOPE("%s: created transaction for a new send: %s", *this, this->_data);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      this->_machine_state_thread.reset(
        new reactor::Thread{
          *reactor::Scheduler::scheduler(),
          "notify fsm update",
          [this, &state]
          {
            this->_notify_on_status_update(state);
          }});
    }

    Transaction::~Transaction()
    {
      if (this->_machine_state_thread)
      {
        this->_machine_state_thread->terminate_now();
        this->_machine_state_thread.reset();
      }
      this->_machine.reset();
    }

    void
    Transaction::_notify_on_status_update(surface::gap::State const& state)
    {
      while (this->_machine != nullptr)
      {
        reactor::Thread& current =
          *reactor::Scheduler::scheduler()->current();

        current.wait(this->_machine->state_changed());
        if (this->last_status(
              _transaction_status(
                *this->data(),
                this->_machine->current_state())))
          state.enqueue(Notification(this->id(), this->last_status()));
      }
    }

    void
    Transaction::accept()
    {
      ELLE_TRACE_SCOPE("%s: accepting transaction", *this);

      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)", *this);
        throw BadOperation(BadOperation::Type::accept);
      }

      if (!dynamic_cast<ReceiveMachine*>(this->_machine.get()))
      {
        ELLE_ERR("%s: accepting on a send machine", *this);
        throw BadOperation(BadOperation::Type::accept);
      }
      static_cast<ReceiveMachine*>(this->_machine.get())->accept();
    }

    void
    Transaction::reject()
    {
      ELLE_TRACE_SCOPE("%s: rejecting transaction", *this);
      ELLE_ASSERT(this->_machine != nullptr);

      if (!dynamic_cast<ReceiveMachine*>(this->_machine.get()))
      {
        ELLE_ERR("%s: reject on a send machine", *this);
        throw BadOperation(BadOperation::Type::reject);
      }

      static_cast<ReceiveMachine*>(this->_machine.get())->reject();
    }

    void
    Transaction::cancel()
    {
      ELLE_TRACE_SCOPE("%s: canceling transaction", *this);
      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)", *this);
        throw BadOperation(BadOperation::Type::cancel);
      }

      this->_machine->cancel();
    }

    void
    Transaction::join()
    {
      ELLE_TRACE_SCOPE("%s: joining transaction", *this);
      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)", *this);
        throw BadOperation(BadOperation::Type::join);
      }

      this->_machine->join();
    }

    bool
    Transaction::last_status(gap_TransactionStatus status)
    {
      if (this->_last_status == status)
        return false;

      this->_last_status = status;
      return true;
    }

    gap_TransactionStatus
    Transaction::last_status() const
    {
      if (this->_last_status == gap_transaction_none)
        return _transaction_status(*this->data(), TransferMachine::State::None);

      return this->_last_status;
    }

    float
    Transaction::progress() const
    {
      ELLE_DEBUG_SCOPE("%s: progress transaction", *this);

      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)", *this);
        throw BadOperation(BadOperation::Type::progress);
      }

      return this->_machine->progress();
    }

    static
    std::vector<infinit::oracles::Transaction::Status> const&
    final_status()
    {
      static std::vector<infinit::oracles::Transaction::Status> final{
        infinit::oracles::Transaction::Status::rejected,
          infinit::oracles::Transaction::Status::finished,
        infinit::oracles::Transaction::Status::canceled,
        infinit::oracles::Transaction::Status::failed};

      return final;
    }

    void
    Transaction::on_transaction_update(surface::gap::State const& state,
                                       Data const&                data)
    {
      ELLE_TRACE_SCOPE("%s: update transaction data with %s", *this, data);

      ELLE_DEBUG(
        "%s, %s my device, %s recipient device, my id %s, recipient id %s",
        *this, state.device().id , data.recipient_device_id, state.me().id,
        data.recipient_id
      );

      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)",
        *this);
        return;
      }

      // If I'm the recipient but no the right device... Sand notification to GUI
      if (state.me().id == data.recipient_id &&
              state.device().id != data.recipient_device_id)
      {
        switch(this->_data->status)
        {
          case Data::Status::initialized:
            if (data.status == Data::Status::accepted)
            {
              ELLE_DEBUG("%s, accepted from somewhere else", *this);
              state.enqueue(Notification(this->id(),
              gap_transaction_accepted_somewhere_else));
            }
           case Data::Status::accepted:
             if (data.status == Data::Status::finished)
             {
               ELLE_DEBUG("%s, finished from somewhere else", *this);
               state.enqueue(Notification(this->id(),
               gap_transaction_finished_somewhere_else));
             }
           default:
            break;
         }
       }

      if (this->_data->status == data.status)
      {
        ELLE_WARN("%s: transaction already has the status %s",
                  *this, this->_data->status);
      }
      if (std::find(final_status().begin(), final_status().end(),
                    this->_data->status) != final_status().end())
      {
        ELLE_WARN("%s: transaction already has a final status %s, can't "\
                  "change to %s", *this, this->_data->status, data.status);
      }
      // XXX: This is totaly wrong, the > is not overload, so the transaction
      // status as to be ordered in the right order.
      else if (this->_data->status > data.status)
      {
        ELLE_WARN("%s: receive a status update (%s) that is lower than the "\
                  " current %s", *this, this->_data->status, data.status);
      }


      *(this->_data) = data;

      this->_data->status = data.status;

      ELLE_DEBUG("%s: updating machine", *this);
      this->_machine->transaction_status_update(this->_data->status);
    }

    void
    Transaction::on_peer_connection_update(
      infinit::oracles::trophonius::PeerConnectionUpdateNotification const& update)
    {
      ELLE_TRACE_SCOPE("%s: update peer status with %s", *this, update);

      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)", *this);
        return;
      }

      ELLE_ASSERT_EQ(this->_data->id, update.transaction_id);

      // XXX.
      // ELLE_ASSERT(
      //   std::find(update.devices.begin(), update.devices.end(),
      //             this->_data->sender_device_id) != update.devices.end());

      // ELLE_ASSERT(
      //   std::find(update.devices.begin(), update.devices.end(),
      //             this->_data->recipient_device_id) != update.devices.end());

      this->_machine->peer_connection_update(update.status);
    }

    bool
    Transaction::concerns_user(std::string const& peer_id) const
    {
      return this->_data->sender_id == peer_id ||
             this->_data->recipient_id == peer_id;
    }

    bool
    Transaction::concerns_device(std::string const& device_id) const
    {
      return this->_data->sender_device_id == device_id ||
             this->_data->recipient_device_id == device_id;
    }

    bool
    Transaction::has_transaction_id(std::string const& id) const
    {
      return this->_data->id == id;
    }

    bool
    Transaction::final() const
    {
      if (this->_machine == nullptr)
        return true;

      if (this->_data == nullptr)
        return true;

      return std::find(final_status().begin(),
                       final_status().end(),
                       this->_data->status) != final_status().end();
    }

    void
    Transaction::print(std::ostream& stream) const
    {
      stream << "Transaction(data: ";
      if (this->_data != nullptr)
        stream << *this->_data;
      else
        stream << "null";
      stream << ", machine: ";
      if (this->_machine != nullptr)
        stream << *this->_machine;
      else
        stream << "null";
      stream << ")";
    }
  }
}
