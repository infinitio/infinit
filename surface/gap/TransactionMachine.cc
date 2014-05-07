#include <surface/gap/TransactionMachine.hh>

#include <functional>
#include <sstream>

#include <elle/AtomicFile.hh>
#include <elle/Backtrace.hh>
#include <elle/container/list.hh>
#include <elle/container/set.hh>
#include <elle/network/Interface.hh>
#include <elle/os/environ.hh>
#include <elle/printf.hh>
#include <elle/serialization/Serializer.hh>
#include <elle/serialization/json.hh>
#include <elle/serialize/insert.hh>

#include <reactor/fsm/Machine.hh>
#include <reactor/exception.hh>
#include <reactor/Scope.hh>

#include <CrashReporter.hh>
#include <common/common.hh>
#include <frete/Frete.hh>
#include <infinit/metrics/CompositeReporter.hh>
#include <papier/Authority.hh>
#include <station/Station.hh>
#include <surface/gap/PeerTransferMachine.hh>
#include <surface/gap/State.hh>

ELLE_LOG_COMPONENT("surface.gap.TransactionMachine");

namespace surface
{
  namespace gap
  {
    /*---------.
    | Snapshot |
    `---------*/

    TransactionMachine::Snapshot::Snapshot(TransactionMachine const& machine)
      : _current_state(machine._machine.current_state()->name())
    {}

    TransactionMachine::Snapshot::Snapshot(
      elle::serialization::SerializerIn& source)
      : _current_state()
    {
      this->serialize(source);
    }

    void
    TransactionMachine::Snapshot::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("current_state", this->_current_state);
    }

    boost::filesystem::path
    TransactionMachine::Snapshot::path(TransactionMachine const& machine)
    {
      boost::filesystem::path path =
        common::infinit::transactions_directory(machine._state.me().id);
      return path / (machine.transaction_id() + ".fsm");
    }

    void
    TransactionMachine::Snapshot::print(std::ostream& stream) const
    {
      elle::fprintf(stream, "TransactionMachin::Snapshot(\"%s\")",
                    this->_current_state);
    }

    /*-------------------.
    | TransactionMachine |
    `-------------------*/

    TransactionMachine::OldSnapshot::OldSnapshot(
      Data const& data,
      State const state,
      std::unordered_set<std::string> const& files,
      std::string const& message):
      data(data),
      state(state),
      files(files),
      message(message)
    {}

    void
    TransactionMachine::OldSnapshot::print(std::ostream& stream) const
    {
      stream << "OldSnapshot(" << this->data << ")";
    }

    //---------- TransactionMachine -----------------------------------------------
    TransactionMachine::TransactionMachine(
      surface::gap::State const& state,
      uint32_t id,
      std::shared_ptr<TransactionMachine::Data> data):
      _snapshot_path(
        common::infinit::transaction_snapshots_directory(state.me().id) /
        boost::filesystem::unique_path()),
      _id(id),
      _machine(elle::sprintf("transaction (%s) fsm", id)),
      _machine_thread(),
      _current_state(State::None),
      _state_changed("state changed"),
      _transfer_core_state(
        this->_machine.state_make(
          "transfer core", std::bind(&TransactionMachine::_transfer_core, this))),
      _finish_state(
        this->_machine.state_make(
          "finish", std::bind(&TransactionMachine::_finish, this))),
      _reject_state(
        this->_machine.state_make(
          "reject", std::bind(&TransactionMachine::_reject, this))),
      _cancel_state(
        this->_machine.state_make(
          "cancel", std::bind(&TransactionMachine::_cancel, this))),
      _fail_state(
        this->_machine.state_make(
          "fail", std::bind(&TransactionMachine::_fail, this))),
      _end_state(
        this->_machine.state_make(
          "end", std::bind(&TransactionMachine::_end, this))),
      _finished("finished"),
      _rejected("rejected"),
      _canceled("canceled"),
      _failed("failed"),
      _station(nullptr),
      _transfer_machine(new PeerTransferMachine(*this)),
      _state(state),
      _data(std::move(data))
    {
      ELLE_TRACE_SCOPE("%s: creating transfer machine: %s", *this, this->_data);

      // Normal way.
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_finish_state,
                                    reactor::Waitables{&this->_finished},
                                    true);

      this->_machine.transition_add(this->_finish_state,
                                    this->_end_state);

      // Cancel way.
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_cancel_state,
                                    this->_end_state);

      // Fail way.
      this->_machine.transition_add_catch(this->_transfer_core_state,
                                          this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_WARN("%s: error while transfering: %s",
                      *this, elle::exception_string(e));
          });

      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_fail_state,
                                    reactor::Waitables{&this->_failed}, true);

      // Reset transfer
      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_transfer_core_state,
                                    reactor::Waitables{&this->_reset_transfer_signal},
                                    true);

      this->_machine.transition_add(this->_fail_state,
                                    this->_end_state);
      // Reject.
      this->_machine.transition_add(this->_reject_state,
                                    this->_end_state);

      // The catch transitions just open the barrier to logging purpose.
      // The snapshot will be kept.
      this->_machine.transition_add_catch(this->_fail_state, this->_end_state)
        .action([this] { ELLE_ERR("%s: failure failed", *this); });
      this->_machine.transition_add_catch(this->_cancel_state, this->_end_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_ERR("%s: cancellation failed: %s",
                     *this, elle::exception_string(e));
            this->_failed.open();
          });
      this->_machine.transition_add_catch(this->_finish_state, this->_end_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_ERR("%s: termination failed: %s",
                     *this, elle::exception_string(e));
            this->_failed.open();
          });

      this->_machine.state_changed().connect(
        [this] (reactor::fsm::State const&) { this->_save_snapshot(); });
    }

    TransactionMachine::~TransactionMachine()
    {
      ELLE_TRACE_SCOPE("%s: destroying transaction machine", *this);
    }

    TransactionMachine::OldSnapshot
    TransactionMachine::_make_snapshot() const
    {
      return OldSnapshot{*this->data(), this->_current_state};
    }

    void
    TransactionMachine::_save_old_snapshot() const
    {
      ELLE_TRACE_SCOPE("%s: save old snapshot to %s",
                       *this, this->_snapshot_path.string());
      auto snapshot = this->_make_snapshot();
      ELLE_DUMP("snapshot data: %s", snapshot);
      elle::serialize::to_file(this->_snapshot_path.string()) << snapshot;
    }

    void
    TransactionMachine::_save_snapshot() const
    {
      if (!this->_data->id.empty())
      {
        boost::filesystem::path path = Snapshot::path(*this);
        ELLE_TRACE_SCOPE("%s: save snapshot to %s", *this, path);
        elle::AtomicFile destination(path);
        destination.write() << [&] (elle::AtomicFile::Write& write)
        {
          elle::serialization::json::SerializerOut output(write.stream());
          Snapshot(*this).serialize(output);
        };
      }
    }

    void
    TransactionMachine::current_state(TransactionMachine::State const& state)
    {
      ELLE_TRACE_SCOPE("%s: set new state to %s at progress %s", *this, state,
                       this->progress());
      this->_current_state = state;
      this->_save_old_snapshot();
      this->_state_changed.signal();
    }

    TransactionMachine::State
    TransactionMachine::current_state() const
    {
      return this->_current_state;
    }

    void
    TransactionMachine::_transfer_core()
    {
      ELLE_TRACE_SCOPE("%s: start transfer core machine", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      try
      {
        this->_transfer_machine->run();
        ELLE_TRACE("%s: core machine finished properly", *this);
      }
      catch (reactor::Terminate const&)
      {
        ELLE_TRACE("%s: terminated", *this);
        throw;
      }
      catch (std::exception const&)
      {
        ELLE_ERR("%s: something went wrong while transfering: %s",
                 *this, elle::exception_string());
        throw;
      }
      if (this->_failed.opened())
        throw Exception(gap_error, "an error occured");
    }

    void
    TransactionMachine::_end()
    {
      ELLE_TRACE_SCOPE("%s: end", *this);
    }

    void
    TransactionMachine::_finish()
    {
      ELLE_TRACE_SCOPE("%s: finish", *this);
      if (!this->is_sender())
      {
        if (this->state().metrics_reporter())
          this->state().metrics_reporter()->transaction_ended(
          this->transaction_id(),
          infinit::oracles::Transaction::Status::finished,
          ""
        );
      }
      this->current_state(State::Finished);
      this->_finalize(infinit::oracles::Transaction::Status::finished);
    }

    void
    TransactionMachine::_reject()
    {
      ELLE_TRACE_SCOPE("%s: reject", *this);
      this->current_state(State::Rejected);
      this->_finalize(infinit::oracles::Transaction::Status::rejected);
    }

    void
    TransactionMachine::_cancel()
    {
      ELLE_TRACE_SCOPE("%s: cancel", *this);
      this->current_state(State::Canceled);
      this->_finalize(infinit::oracles::Transaction::Status::canceled);
    }

    void
    TransactionMachine::_fail()
    {
      ELLE_TRACE_SCOPE("%s: fail", *this);
      std::string transaction_id;
      if (!this->data()->id.empty())
        transaction_id = this->transaction_id();
      else
        transaction_id = "unknown";
      if (this->state().metrics_reporter())
        this->state().metrics_reporter()->transaction_ended(
        transaction_id,
        infinit::oracles::Transaction::Status::failed,
        ""
      );
      // Send report for failed transfer
      elle::crash::transfer_failed_report(this->state().meta().protocol(),
                                          this->state().meta().host(),
                                          this->state().meta().port(),
                                          this->state().me().email);
      this->current_state(State::Failed);
      this->_finalize(infinit::oracles::Transaction::Status::failed);
    }

    void
    TransactionMachine::_finalize(infinit::oracles::Transaction::Status status)
    {
      ELLE_TRACE_SCOPE("%s: finalize machine: %s", *this, status);
      if (!this->_data->empty())
      {
        try
        {
          this->state().meta().update_transaction(
            this->transaction_id(), status);
        }
        catch (infinit::oracles::meta::Exception const& e)
        {
          using infinit::oracles::meta::Error;
          if (e.err == Error::transaction_already_finalized)
            ELLE_TRACE("%s: transaction already finalized", *this);
          else if (e.err == Error::transaction_already_has_this_status)
            ELLE_TRACE("%s: transaction already in this state", *this);
          else
            ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                     *this, this->transaction_id(), elle::exception_string());
        }
        catch (std::exception const&)
        {
          ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                   *this, this->transaction_id(), elle::exception_string());
        }
      }
      else
      {
        ELLE_ERR("%s: can't finalize transaction: id is still empty", *this);
      }
      this->cleanup();
    }

    void
    TransactionMachine::_run(reactor::fsm::State& initial_state)
    {
      ELLE_TRACE_SCOPE("%s: running transfer machine", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      auto& scheduler = *reactor::Scheduler::scheduler();
      this->_machine_thread.reset(
        new reactor::Thread{
          scheduler,
          "run",
          [&]
          {
            this->_machine.run(initial_state);
            ELLE_TRACE("%s: machine finished properly", *this);
            boost::filesystem::remove(this->_snapshot_path);
          }});
    }

    void
    TransactionMachine::peer_available(
      std::vector<std::pair<std::string, int>> const& endpoints)
    {
      ELLE_TRACE_SCOPE("%s: peer is available for peer to peer connection",
                       *this);
      this->_transfer_machine->peer_available(endpoints);
    }

    void
    TransactionMachine::peer_unavailable()
    {
      ELLE_TRACE_SCOPE("%s: peer is unavailable for peer to peer connection",
                       *this);
      this->_transfer_machine->peer_unavailable();
    }

    void
    TransactionMachine::peer_connection_changed(bool user_status)
    {
      ELLE_TRACE_SCOPE("%s: update with new peer connection status %s",
                       *this, user_status);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      if (user_status)
        ELLE_DEBUG("%s: peer is now online", *this)
        {
          this->_transfer_machine->peer_offline().close();
          this->_transfer_machine->peer_online().open();
        }
      else
        ELLE_DEBUG("%s: peer is now offline", *this)
        {
          this->_transfer_machine->peer_online().close();
          this->_transfer_machine->peer_offline().open();
        }
    }

    void
    TransactionMachine::cancel()
    {
      ELLE_TRACE_SCOPE("%s: cancel transaction %s", *this, this->data()->id);
      if (!this->_canceled.opened())
      {
        if (this->state().metrics_reporter())
          this->state().metrics_reporter()->transaction_ended(
            this->transaction_id(),
            infinit::oracles::Transaction::Status::canceled,
            ""
            );
      }
      this->_canceled.open();
    }

    bool
    TransactionMachine::pause()
    {
      ELLE_TRACE_SCOPE("%s: pause transaction %s", *this, this->data()->id);
      throw elle::Exception(
        elle::sprintf("%s: pause not implemented yet", *this));
    }

    void
    TransactionMachine::interrupt()
    {
      ELLE_TRACE_SCOPE("%s: interrupt transaction %s", *this, this->data()->id);
      throw elle::Exception(
        elle::sprintf("%s: interruption not implemented yet", *this));
    }

    bool
    TransactionMachine::concerns_transaction(std::string const& transaction_id)
    {
      return this->_data->id == transaction_id;
    }

    bool
    TransactionMachine::concerns_user(std::string const& user_id)
    {
      return (user_id == this->_data->sender_id) ||
             (user_id == this->_data->recipient_id);
    }

    bool
    TransactionMachine::concerns_device(std::string const& device_id)
    {
      return (device_id == this->_data->sender_device_id) ||
             (device_id == this->_data->recipient_device_id);
    }

    bool
    TransactionMachine::has_id(uint32_t id)
    {
      return (id == this->id());
    }

    void
    TransactionMachine::join()
    {
      ELLE_TRACE_SCOPE("%s: join machine", *this);
      if (this->_machine_thread == nullptr)
      {
        ELLE_WARN("%s: thread already destroyed", *this);
        return;
      }
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      reactor::Thread* current = reactor::Scheduler::scheduler()->current();
      ELLE_ASSERT(current != nullptr);
      ELLE_DEBUG("%s: start joining", *this);
      if (this->_machine_thread.get() != nullptr)
      {
        current->wait(*this->_machine_thread.get());
      }
      ELLE_DEBUG("%s: successfully joined", *this);
    }

    void
    TransactionMachine::_stop()
    {
      ELLE_TRACE_SCOPE("%s: stop machine for transaction", *this);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      if (this->_machine_thread != nullptr)
      {
        ELLE_DEBUG("%s: terminate machine thread", *this)
          this->_machine_thread->terminate_now();
        this->_machine_thread.reset();
      }
    }

    /*-----------.
    | Attributes |
    `-----------*/

    std::string const&
    TransactionMachine::transaction_id() const
    {
      if (this->_data->id.empty())
        throw elle::Exception(
          elle::sprintf("%s: Transaction machine is not ready", *this));
      return this->_data->id;
    }

    void
    TransactionMachine::transaction_id(std::string const& id)
    {
      if (!this->_data->id.empty())
      {
        ELLE_ASSERT_EQ(this->_data->id, id);
        return;
      }
      this->_data->id = id;
    }

    std::string const&
    TransactionMachine::peer_id() const
    {
      if (this->is_sender())
      {
        ELLE_ASSERT(!this->_data->recipient_id.empty());
        return this->_data->recipient_id;
      }
      else
      {
        ELLE_ASSERT(!this->_data->sender_id.empty());
        return this->_data->sender_id;
      }
    }

    void
    TransactionMachine::peer_id(std::string const& id)
    {
      if (this->is_sender())
      {
        if (!this->_data->recipient_id.empty() && this->_data->recipient_id != id)
          ELLE_WARN("%s: replace recipient id %s by %s",
                    *this, this->_data->recipient_id, id);
        this->_data->recipient_id = id;
      }
      else
      {
        if (!this->_data->sender_id.empty() && this->_data->sender_id != id)
          ELLE_WARN("%s: replace sender id %s by %s",
                    *this, this->_data->sender_id, id);
        this->_data->sender_id = id;
      }
    }

    station::Station&
    TransactionMachine::station()
    {
      if (!this->_station)
      {
        ELLE_TRACE_SCOPE("%s: building station", *this);
        this->_station.reset(
          new station::Station(
            papier::authority(),
            this->state().passport(),
            elle::sprintf("Station(id=%s, tr=%s)", this->id(), this->_data->id)
          ));
      }
      ELLE_ASSERT(this->_station != nullptr);
      return *this->_station;
    }

    float
    TransactionMachine::progress() const
    {
      return this->_transfer_machine->progress();
    }

    /*----------.
    | Printable |
    `----------*/

    void
    TransactionMachine::print(std::ostream& stream) const
    {
      auto const& data = *this->_data;
      auto const& me = this->state().me();
      stream << elle::demangle(typeid(*this).name())
             << "(id=" << this->id() << ", "
             << "(u=" << me.id;
      if (!data.id.empty())
        stream << ", t=" << data.id;
      stream << ")";
    }

    std::function<aws::Credentials(bool)>
    TransactionMachine::make_aws_credentials_getter()
    {
      return [this](bool first_time)
      {
         auto& meta = this->state().meta();
         auto token = meta.get_cloud_buffer_token(this->transaction_id(),
                                                  !first_time);//force-regenerate
         auto credentials = aws::Credentials(token.access_key_id,
                                             token.secret_access_key,
                                             token.session_token,
                                             token.region,
                                             token.bucket,
                                             token.folder,
                                             token.expiration);
         return credentials;
      };
    }

    void
    TransactionMachine::reset_transfer()
    {
      this->reset_transfer_signal().signal();
    }

    std::ostream&
    operator <<(std::ostream& out,
                TransactionMachine::State const& t)
    {
      switch (t)
      {
        case TransactionMachine::State::NewTransaction:
          return out << "NewTransaction";
        case TransactionMachine::State::SenderCreateTransaction:
          return out << "SenderCreateTransaction";
        case TransactionMachine::State::SenderWaitForDecision:
          return out << "SenderWaitForDecision";
        case TransactionMachine::State::RecipientWaitForDecision:
          return out << "RecipientWaitForDecision";
        case TransactionMachine::State::RecipientAccepted:
          return out << "RecipientAccepted";
        case TransactionMachine::State::PublishInterfaces:
          return out << "PublishInterfaces";
        case TransactionMachine::State::Connect:
          return out << "Connect";
        case TransactionMachine::State::PeerDisconnected:
          return out << "PeerDisconnected";
        case TransactionMachine::State::PeerConnectionLost:
          return out << "PeerConnectionLost";
        case TransactionMachine::State::Transfer:
          return out << "Transfer";
        case TransactionMachine::State::Finished:
          return out << "Finished";
        case TransactionMachine::State::Rejected:
          return out << "Rejected";
        case TransactionMachine::State::Canceled:
          return out << "Canceled";
        case TransactionMachine::State::Failed:
          return out << "Failed";
        case TransactionMachine::State::Over:
          return out << "Over";
        case TransactionMachine::State::CloudBuffered:
          return out << "CloudBuffered";
        case TransactionMachine::State::CloudBufferingBeforeAccept:
          return out << "CloudBufferingBeforeAccept";
        case TransactionMachine::State::None:
          return out << "None";
        case TransactionMachine::State::GhostCloudBuffering:
          return out << "GhostCloudBuffering";
        case TransactionMachine::State::GhostCloudBufferingFinished:
          return out << "GhostCloudBufferingFinished";
        case TransactionMachine::State::DataExhausted:
          return out << "DataExhausted";
        case TransactionMachine::State::CloudSynchronize:
          return out << "CloudSynchronize";
      }
      return out;
    }
  }
}
