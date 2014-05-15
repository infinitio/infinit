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
      return machine.transaction().snapshots_directory() / "fsm.snapshot";
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

    TransactionMachine::TransactionMachine(
      Transaction& transaction,
      uint32_t id,
      std::shared_ptr<TransactionMachine::Data> data):
      _id(id),
      _machine(elle::sprintf("transaction (%s) fsm", id)),
      _machine_thread(),
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
      _transaction(transaction),
      _state(transaction.state()),
      _data(std::move(data))
    {
      ELLE_TRACE_SCOPE("%s: create transfaction machine", *this);

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

    void
    TransactionMachine::_save_snapshot() const
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

    void
    TransactionMachine::gap_state(gap_TransactionStatus v)
    {
      ELLE_TRACE("%s: change GAP status to %s", *this, v);
      this->state().enqueue(Transaction::Notification(this->id(), v));
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
      boost::system::error_code error;
      auto path = this->transaction().snapshots_directory();
      boost::filesystem::remove_all(path, error);
      if (error)
        ELLE_WARN("%s: unable to remove snapshot directory %s", *this, path);
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
      this->gap_state(gap_transaction_finished);
      this->_finalize(infinit::oracles::Transaction::Status::finished);
    }

    void
    TransactionMachine::_reject()
    {
      ELLE_TRACE_SCOPE("%s: reject", *this);
      this->gap_state(gap_transaction_rejected);
      this->_finalize(infinit::oracles::Transaction::Status::rejected);
    }

    void
    TransactionMachine::_cancel()
    {
      ELLE_TRACE_SCOPE("%s: cancel", *this);
      this->gap_state(gap_transaction_canceled);
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
      this->gap_state(gap_transaction_failed);
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
      ELLE_TRACE_SCOPE("%s: start transfaction machine at %s",
                       *this, initial_state);
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
    TransactionMachine::notify_user_connection_status(
      std::string const& user_id,
      std::string const& device_id,
      bool online)
    {
      if (user_id == this->_data->sender_id)
      // We don't care about our own status.
      if (user_id == this->transaction().state().me().id)
        return;
      // Check it concerns one of the users.
      ELLE_ASSERT(user_id == this->_data->sender_id
                  || user_id == this->_data->recipient_id);
      // If recipient, check it isn't accepted or it's for the right device.
      ELLE_ASSERT(user_id != this->_data->recipient_id
                  || this->_data->recipient_device_id.empty()
                  || device_id == this->_data->recipient_device_id);
      // If sender, check  it's for the right device.
      ELLE_ASSERT(user_id != this->_data->sender_id
                  || device_id == this->_data->sender_device_id);
      this->peer_connection_changed(online);
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
         int delay = 1;
         oracles::meta::CloudBufferTokenResponse token;
         while (true)
         {
           try
           {
             token = meta.get_cloud_buffer_token(this->transaction_id(),
                                                      !first_time);//force-regenerate
             break;
           }
           catch(reactor::Terminate const& e)
           {
             throw;
           }
           catch(...)
           {
             ELLE_LOG("%s: get_cloud_buffer_token failed with %s, retrying...",
                      *this, elle::exception_string());
             // if meta looses connectivity to provider let's not flood it
             reactor::sleep(boost::posix_time::seconds(delay));
             delay = std::min(delay * 2, 60 * 10);
           }
         }
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
    std::pair<std::string, bool>
    TransactionMachine::archive_info()
    {
      auto const& files = this->data()->files;
      if (files.size() == 1)
        if (this->data()->is_directory)
          return std::make_pair(
            boost::filesystem::path(*files.begin())
               .filename()
               .replace_extension("zip")
               .string(),
            true);
        else
          return std::make_pair(*files.begin(), false);
      else
        return std::make_pair("archive.zip", true);
    }

  }
}
