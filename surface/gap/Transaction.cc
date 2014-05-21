#include <surface/gap/Transaction.hh>

#include <memory>

#include <elle/AtomicFile.hh>
#include <elle/Error.hh>
#include <elle/serialization/json.hh>

#include <common/common.hh>
#include <surface/gap/Exception.hh>
#include <surface/gap/LinkSendMachine.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/PeerSendMachine.hh>
#include <surface/gap/TransactionMachine.hh>
#include <surface/gap/enums.hh>

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

    std::vector<infinit::oracles::Transaction::Status>
    Transaction::final_statuses{
      infinit::oracles::Transaction::Status::rejected,
      infinit::oracles::Transaction::Status::finished,
      infinit::oracles::Transaction::Status::canceled,
      infinit::oracles::Transaction::Status::failed
    };

    gap_TransactionStatus
    Transaction::_transaction_status(Transaction::Data const& data) const
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
          return gap_transaction_new;
      }
    }

    // - Exception -------------------------------------------------------------
    Transaction::BadOperation::BadOperation(Type type):
      Exception(gap_error, elle::sprintf("%s", type)),
      _type(type)
    {}

    /*---------.
    | Snapshot |
    `---------*/

    void
    Transaction::_snapshot_save() const
    {
      create_directories(this->_snapshots_directory);
      ELLE_TRACE_SCOPE("%s: save snapshot to %s", *this, this->_snapshot_path);
      Snapshot snapshot(this->_sender,
                        this->_data,
                        this->_archived,
                        this->_files,
                        this->_message);
      ELLE_DUMP("%s: snapshot data: %s", *this, snapshot);
      elle::AtomicFile destination(this->_snapshot_path);
      destination.write() << [&] (elle::AtomicFile::Write& write)
      {
        elle::serialization::json::SerializerOut output(write.stream());
        snapshot.serialize(output);
      };
    }

    Transaction::Snapshot::Snapshot(
      bool sender,
      std::shared_ptr<Data> data,
      bool archived,
      boost::optional<std::vector<std::string>> files,
      boost::optional<std::string> message)
      : _sender(sender)
      , _data(data)
      , _files(files)
      , _message(message)
      , _archived(archived)
    {}

    Transaction::Snapshot::Snapshot(
      elle::serialization::SerializerIn& serializer)
    {
      this->serialize(serializer);
    }

    void
    Transaction::Snapshot::serialize(
      elle::serialization::Serializer& serializer)
    {
      serializer.serialize("data", this->_data);
      serializer.serialize("sender", this->_sender);
      serializer.serialize("files", this->_files);
      serializer.serialize("message", this->_message);
      serializer.serialize("archived", this->_archived);
    }

    void
    Transaction::Snapshot::print(std::ostream& stream) const
    {
      elle::fprintf(stream, "Snapshot()");
    }

    /*-------------.
    | Construction |
    `-------------*/

    // Construct to send files.
    Transaction::Transaction(State& state,
                             uint32_t id,
                             std::string const& peer_id,
                             std::vector<std::string> files,
                             std::string const& message)
      // FIXME: ensure better uniqueness.
      : _snapshots_directory(
        boost::filesystem::path(common::infinit::user_directory(state.me().id))
        / "transactions" / boost::filesystem::unique_path())
      , _snapshot_path(this->_snapshots_directory / "transaction.snapshot")
      , _state(state)
      , _files(std::move(files))
      , _message(message)
      , _archived(false)
      , _id(id)
      , _sender(true)
      , _data(nullptr)
      , _machine(nullptr)
      , _over(false)
    {
      auto data = std::make_shared<infinit::oracles::PeerTransaction>(
        state.me().id,
        state.me().fullname,
        state.device().id,
        peer_id);
      this->_data = data;
      this->_machine.reset(
        new PeerSendMachine(*this, this->_id, peer_id,
                            this->_files.get(), message, data));
      ELLE_TRACE_SCOPE("%s: construct to send %s files",
                       *this, this->_files.get().size());
      this->_snapshot_save();
    }

    // Construct to create link.
    Transaction::Transaction(State& state,
                             uint32_t id,
                             std::vector<std::string> files,
                             std::string const& message)
      // FIXME: ensure better uniqueness.
      : _snapshots_directory(
        boost::filesystem::path(common::infinit::user_directory(state.me().id))
        / "transactions" / boost::filesystem::unique_path())
      , _snapshot_path(this->_snapshots_directory / "transaction.snapshot")
      , _state(state)
      , _files(std::move(files))
      , _message(message)
      , _archived(false)
      , _id(id)
      , _sender(true)
      , _data(nullptr)
      , _machine(nullptr)
      , _over(false)
    {
      ELLE_TRACE_SCOPE("%s: construct to generate link for %s",
                       *this, this->_files.get());
      auto data = std::make_shared<infinit::oracles::LinkTransaction>();
      data->sender_id = state.me().id;
      data->sender_device_id = state.device().id;
      this->_data = data;
      this->_machine.reset(
        new LinkSendMachine(*this, this->_id, this->_files.get(), data));
      this->_snapshot_save();
    }

    // FIXME: Split history transactions.
    Transaction::Transaction(State& state,
                             uint32_t id,
                             std::shared_ptr<Data> data,
                             bool history)
      : _snapshots_directory(
        boost::filesystem::path(common::infinit::user_directory(state.me().id))
        / "transactions" / boost::filesystem::unique_path())
      , _snapshot_path(this->_snapshots_directory / "transaction.snapshot")
      , _state(state)
      , _files()
      , _message()
      , _archived(false)
      , _id(id)
      , _sender(state.me().id == data->sender_id &&
              state.device().id == data->sender_device_id)
      , _data(data)
      , _machine()
      , _over(history)
    {
      ELLE_TRACE_SCOPE("%s: constructed from data", *this);
      if (history)
      {
        ELLE_DEBUG("%s: part of history", *this);
        return;
      }
      auto me = state.me().id;
      auto device = state.device().id;
      auto sender =
        me == this->_data->sender_id && device == this->_data->sender_device_id;
      if (auto peer_data =
          std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
            this->_data))
      {
        auto recipient =
          me == peer_data->recipient_id &&
          (peer_data->recipient_device_id.empty() ||
           device == peer_data->recipient_device_id);
        if (sender)
        {
          ELLE_WARN("%s: can't restore peer send transaction from server",
                    *this);
          throw elle::Error("can't restore peer send transaction from server");
        }
        else if (recipient)
        {
          ELLE_DEBUG("%s: start receive machine", *this);
          this->_machine.reset(new ReceiveMachine(*this, this->_id, peer_data));
        }
        else
          ELLE_DEBUG("%s: not for our device: %s", *this, state.device().id);
        if (sender || recipient)
          this->_snapshot_save();
      }
      else if (auto link_data =
          std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
            this->_data))
      {
          throw elle::Error("can't restore link send transaction from server");
      }
      else
      {
        ELLE_ERR("%s: unkown transaction type: %s",
                 *this, elle::demangle(typeid(*this->_data).name()));
      }
    }

    Transaction::Transaction(State& state,
                             uint32_t id,
                             Snapshot snapshot,
                             boost::filesystem::path snapshots_directory)
      : _snapshots_directory(std::move(snapshots_directory))
      , _snapshot_path(this->_snapshots_directory / "transaction.snapshot")
      , _state(state)
      , _files(snapshot.files())
      , _message(snapshot.message())
      , _archived(snapshot.archived())
      , _id(id)
      , _sender(snapshot.sender())
      , _data(snapshot.data())
      , _machine()
      , _over(false)
    {
      ELLE_TRACE_SCOPE("%s: constructed from snapshot %s",
                       *this, this->_snapshot_path);
      if (auto peer_data =
          std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
            this->_data))
      {
        if (this->_sender)
        {
          ELLE_ASSERT(this->_message);
          ELLE_ASSERT(this->_files);
          ELLE_TRACE("%s: create peer send machine", *this)
            this->_machine.reset(
              new PeerSendMachine(*this, this->_id, this->_files.get(),
                                  this->_message.get(), peer_data));
        }
        else
        {
          ELLE_TRACE("%s: create receive machine", *this)
            this->_machine.reset(
              new ReceiveMachine(*this, this->_id, peer_data));
        }
      }
      else if (auto link_data =
               std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
                 this->_data))
      {
        ELLE_ASSERT(this->_files);
        ELLE_TRACE("%s: create link send machine", *this)
          this->_machine.reset(
            new LinkSendMachine(*this, this->_id,
                                this->_files.get(), link_data));
      }
      else
        ELLE_ERR("%s: don't know what to do with a %s",
                 *this, elle::demangle(typeid(*this->_data).name()));
    }

    Transaction::~Transaction()
    {
      this->_machine.reset();
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
      // FIXME
      if (auto machine = dynamic_cast<ReceiveMachine*>(this->_machine.get()))
        machine->accept();
      else
      {
        ELLE_ERR("%s: accepting on a send machine", *this);
        throw BadOperation(BadOperation::Type::accept);
      }
    }

    void
    Transaction::reject()
    {
      ELLE_TRACE_SCOPE("%s: rejecting transaction", *this);
      ELLE_ASSERT(this->_machine != nullptr);
      if (auto machine = dynamic_cast<ReceiveMachine*>(this->_machine.get()))
        machine->reject();
      else
      {
        ELLE_ERR("%s: reject on a send machine", *this);
        throw BadOperation(BadOperation::Type::reject);
      }
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

    gap_TransactionStatus
    Transaction::status() const
    {
      if (this->_machine)
        return this->_machine->gap_status();
      typedef infinit::oracles::Transaction::Status Status;
      switch (this->_data->status)
      {
        // If we have no machine, those are for another device, which should be
        // handled.
        case Status::accepted:
        case Status::created:
        case Status::initialized:
          // FIXME
          return gap_transaction_new;
        // FIXME: What is even this ?
        case Status::none:
        case Status::started:
          return gap_transaction_new;
        // Final states.
        case Status::canceled:
          return gap_transaction_canceled;
        case Status::failed:
          return gap_transaction_failed;
        case Status::finished:
          return gap_transaction_finished;
        case Status::rejected:
          return gap_transaction_rejected;
        default:
          elle::unreachable();
      }
    }

    float
    Transaction::progress() const
    {
      ELLE_DUMP_SCOPE("%s: progress transaction", *this);
      if (this->_machine == nullptr)
      {
        ELLE_WARN("%s: machine is empty (it doesn't concern your device)",
                  *this);
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

    void
    Transaction::on_transaction_update(std::shared_ptr<Data> data)
    {
      ELLE_TRACE_SCOPE("%s: update data with %s", *this, *data);
      this->_data = data;
      if (this->_machine && !this->_over)
      {
        ELLE_DEBUG("%s: updating machine", *this)
          this->_machine->transaction_status_update(this->_data->status);
        this->_snapshot_save();
      }
    }

    void
    Transaction::notify_user_connection_status(std::string const& user_id,
                                               std::string const& device_id,
                                               bool status)
    {
      if (this->_machine == nullptr)
        return;
      this->_machine->notify_user_connection_status(user_id, device_id, status);
    }

    void
    Transaction::notify_peer_reachable(
      std::vector<std::pair<std::string, int>> const& endpoints)
    {
      if (!this->_machine)
      {
        // FIXME: meta send those notifications to all devices, so this is
        // triggered.
        ELLE_ERR(
          "%s: got reachability notification inactive transaction %s",
          *this, this->data()->id);
        return;
      }
      this->_machine->peer_available(endpoints);
    }

    void
    Transaction::notify_peer_unreachable()
    {
      if (!this->_machine)
      {
        // FIXME: meta send those notifications to all devices, so this is
        // triggered.
        ELLE_ERR(
          "%s: got reachability notification for inactive transaction %s",
          *this, this->data()->id);
        return;
      }
      this->_machine->peer_unavailable();
    }

    bool
    Transaction::has_transaction_id(std::string const& id) const
    {
      return this->_data->id == id;
    }

    bool
    Transaction::final() const
    {
      ELLE_ASSERT(this->_data.get());
      return std::find(Transaction::final_statuses.begin(),
                       Transaction::final_statuses.end(),
                       this->_data->status) != Transaction::final_statuses.end();
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
    Transaction::reset()
    {
      ELLE_TRACE_SCOPE("reseting %s", *this);
      if (this->final())
      {
        // Finalized (current session or history transaction): nothing to do.
        ELLE_DEBUG("transaction already finalized");
        return;
      }
      if (this->_machine != nullptr)
        this->_machine->reset_transfer();
    }

    void
    Transaction::archived(bool v)
    {
      this->_archived = v;
      this->_snapshot_save();
    }
  }
}
