#include <surface/gap/Transaction.hh>
#include <surface/gap/enums.hh>

#include <surface/gap/TransactionMachine.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/SendMachine.hh>
#include <surface/gap/Exception.hh>

ELLE_LOG_COMPONENT("surface.gap.Transaction");

namespace surface
{
  namespace gap
  {
    Notification::Type Transaction::Notification::type =
      NotificationType_TransactionUpdate;
    Transaction::Notification::Notification(uint32_t id,
                                            gap_TransactionStatus status):
      id(id),
      status(status)
    {}

    gap_TransactionStatus
    Transaction::_transaction_status(Transaction::Data const& data,
                                     TransactionMachine::State state) const
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
            case TransactionMachine::State::NewTransaction:
            case TransactionMachine::State::SenderCreateTransaction:
            case TransactionMachine::State::None:
            case TransactionMachine::State::Over:
              // The sender is pending creating the transaction.
              return gap_transaction_new;
            case TransactionMachine::State::SenderWaitForDecision:
            case TransactionMachine::State::RecipientWaitForDecision:
            case TransactionMachine::State::RecipientAccepted:
            case TransactionMachine::State::GhostCloudBufferingFinished:
              return gap_transaction_waiting_accept;
            case TransactionMachine::State::CloudSynchronize:
            case TransactionMachine::State::PeerConnectionLost:
            case TransactionMachine::State::PeerDisconnected:
            case TransactionMachine::State::PublishInterfaces:
              // If the progress is full but the transaction is not finished
              // yet, it must be cloud buffered.
              if (this->progress() == 1)
                return gap_transaction_cloud_buffered;
              return gap_transaction_connecting;
            case TransactionMachine::State::Connect:
              return gap_transaction_connecting;
            case TransactionMachine::State::DataExhausted:
              return gap_transaction_waiting_data;
            case TransactionMachine::State::Transfer:
            case TransactionMachine::State::CloudBufferingBeforeAccept:
            case TransactionMachine::State::GhostCloudBuffering:
              return gap_transaction_transferring;
            case TransactionMachine::State::Finished:
              return gap_transaction_finished;
            case TransactionMachine::State::Rejected:
              return gap_transaction_rejected;
            case TransactionMachine::State::Canceled:
              return gap_transaction_canceled;
            case TransactionMachine::State::Failed:
              return gap_transaction_failed;
            case TransactionMachine::State::CloudBuffered:
              return gap_transaction_cloud_buffered;
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
      _sender(state.me().id == data.sender_id &&
              state.device().id == data.sender_device_id),
      _data(new Data{std::move(data)}),
      _machine(),
      _last_status(gap_TransactionStatus(-1))
    {
      ELLE_TRACE_SCOPE("%s: constructed from data", *this);
      ELLE_ASSERT(state.me().id == this->_data->sender_id ||
                  state.me().id == this->_data->recipient_id);
      if (history)
      {
        ELLE_DEBUG("%s: part of history", *this);
        return;
      }
      else if (state.me().id == this->_data->sender_id &&
               state.device().id == this->_data->sender_device_id)
      {
        ELLE_TRACE("%s: start send machine", *this);
        this->_machine.reset(new SendMachine{state, this->_id, this->_data});
      }
      else if (state.me().id == this->_data->recipient_id &&
               (this->_data->recipient_device_id.empty() ||
                state.device().id == this->_data->recipient_device_id))
      {
        ELLE_TRACE("%s: start receive machine", *this);
        this->_machine.reset(new ReceiveMachine{state, this->_id, this->_data});
      }
      else
        ELLE_DEBUG("%s: not for our device: %s", *this, state.device().id);
      // Start a thread to forward GUI states change.
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
                             TransactionMachine::Snapshot snapshot):
      _id(id),
      _sender(state.me().id == snapshot.data.sender_id &&
              state.device().id == snapshot.data.sender_device_id),
      _data(new Data{std::move(snapshot.data)}),
      _machine(),
      _last_status(gap_TransactionStatus(-1))
    {
      ELLE_TRACE_SCOPE("%s: constructed from snapshot (%s)",
                       *this, snapshot.state);
      if (state.me().id == this->_data->sender_id &&
          state.device().id == this->_data->sender_device_id)
      {
        ELLE_TRACE("%s: create send machine", *this)
          this->_machine.reset(
            new SendMachine(state, this->_id, snapshot.files,
                            snapshot.state, snapshot.message, this->_data));
      }
      else if (state.me().id == this->_data->recipient_id &&
               (this->_data->recipient_device_id.empty() ||
                state.device().id == this->_data->recipient_device_id))
      {
        ELLE_TRACE("%s: create receive machine", *this)
          this->_machine.reset(
            new ReceiveMachine(state, this->_id, snapshot.state, this->_data));
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
      _sender(true),
      _data(new Data{state.me().id, state.me().fullname, state.device().id}),
      _machine(new SendMachine{
          state, this->_id, peer_id, std::move(files), message, this->_data}),
      _last_status(gap_TransactionStatus(-1))
    {
      ELLE_TRACE_SCOPE("%s: constructed for send", *this);
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
              this->_transaction_status(
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
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)",
                  *this);
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
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)",
                  *this);
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
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)",
                  *this);
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
      if (this->_last_status == gap_TransactionStatus(-1))
        return this->_transaction_status(*this->data(),
                                         TransactionMachine::State::None);
      return this->_last_status;
    }

    float
    Transaction::progress() const
    {
      ELLE_DUMP_SCOPE("%s: progress transaction", *this);
      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)", *
                  this);
        throw BadOperation(BadOperation::Type::progress);
      }
      return this->_machine->progress();
    }

    bool
    Transaction::pause()
    {
      ELLE_WARN("%s: pause not implemented yet", *this);
      throw BadOperation(BadOperation::Type::pause);
    }

    void
    Transaction::interrupt()
    {
      ELLE_WARN("%s: interruption not implemented yet", *this);
      throw BadOperation(BadOperation::Type::interrupt);
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
    Transaction::on_transaction_update(Data const& data)
    {
      ELLE_TRACE_SCOPE("%s: update transaction data with %s", *this, data);
      if (std::find(final_status().begin(), final_status().end(),
                    this->_data->status) != final_status().end())
      {
        ELLE_WARN("%s: transaction already has a final status %s, can't "\
                  "change to %s", *this, this->_data->status, data.status);
      }
      // XXX: This is totally wrong, the > is not overloaded, so the transaction
      // status has to be ordered.
      else if (this->_data->status > data.status)
      {
        ELLE_WARN("%s: receive a status update (%s) that is lower than the "
                  " current %s", *this, this->_data->status, data.status);
      }
      *(this->_data) = data;
      if (this->_machine)
        ELLE_DEBUG("%s: updating machine", *this)
          this->_machine->transaction_status_update(this->_data->status);
    }

    using infinit::oracles::trophonius::PeerReachabilityNotification;
    void
    Transaction::on_peer_reachability_updated(
      PeerReachabilityNotification const& update)
    {
      // If the transaction is not running, ignore.
      if (this->_machine == nullptr)
        return;
      ELLE_TRACE_SCOPE("%s: peer now %savaialble for peer to peer connection",
                       *this, update.status ? "" : "un");
      ELLE_ASSERT_EQ(this->_data->id, update.transaction_id);
      // Notification should only be sent to the sender device and the
      // recipient device concerned by this transaction.
      ELLE_DEBUG("notify machine of the peer availability change")
        this->peer_availability_status(update.status);
    }

    void
    Transaction::peer_availability_status(bool status)
    {
      this->_machine->peer_availability_changed(status);
    }

    using infinit::oracles::trophonius::UserStatusNotification;
    void
    Transaction::on_peer_connection_status_updated(
      UserStatusNotification const& update)
    {
      ELLE_TRACE_SCOPE(
        "%s: peer went %sline on device %s",
          *this, update.device_status ? "on" : "off", update.device_id);
      // If the transaction is not running, ignore.
      if (this->_machine == nullptr)
      {
        ELLE_DEBUG("%s: no machine to notify", *this);
        return;
      }
      ELLE_ASSERT(this->concerns_user(update.user_id));
      // XXX: There is no way to know if you are the sender or the recipient
      // because state is not accessible from transaction.
      // So we assume that the notification CAN'T be our own status.
      bool is_sender = this->is_sender(update.user_id);
      if (!is_sender && this->_data->recipient_device_id.empty())
      {
        // Transaction has not recipient device id set (transaction is not
        //  accepted) so his general connection status is important.
        ELLE_DEBUG("no recipient device id set, user connection status used")
          this->peer_connection_status(update.status);
      }
      // Check if the transaction is linked to this update, looking for the
      // device_id in both sender and recipient device ids.
      if ((is_sender && this->_data->sender_device_id == update.device_id) ||
          (!is_sender && this->_data->recipient_device_id  == update.device_id))
      {
        ELLE_DEBUG("notify machine of the device connection status update")
          this->peer_connection_status(update.device_status);
      }
      else
      {
        ELLE_DEBUG("device not linked to the transaction");
      }
    }

    void
    Transaction::peer_connection_status(bool status)
    {
      this->_machine->peer_connection_changed(status);
    }

    bool
    Transaction::concerns_user(std::string const& peer_id) const
    {
      return this->_data->sender_id == peer_id ||
             this->_data->recipient_id == peer_id;
    }

    bool
    Transaction::is_sender(std::string const& user_id) const
    {
      return this->concerns_user(user_id) && this->_data->sender_id == user_id;
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
      if (this->_data != nullptr)
        stream << *this->_data;
      else
        stream << "Transaction(null)";
      stream << "(" << this->_id << ")";
    }

    void
    Transaction::reset(surface::gap::State const& state)
    {
      if (this->_machine == nullptr)
        return; // history transaction, nothing going on
      ELLE_TRACE("Reseting %s", *this);
      if (this->_machine_state_thread)
      {
        this->_machine_state_thread->terminate_now();
        this->_machine_state_thread.reset();
      }
      this->_machine.reset();
      if (this->_sender)
        this->_machine.reset(new SendMachine{state, this->_id, this->_data});
      else
        this->_machine.reset(new ReceiveMachine{state, this->_id, this->_data});
    }
  }
}
