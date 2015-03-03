#include <surface/gap/TransactionMachine.hh>

#include <functional>
#include <sstream>

#include <elle/AtomicFile.hh>
#include <elle/Backtrace.hh>
#include <elle/Error.hh>
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
      if (!boost::filesystem::exists(machine.transaction().snapshots_directory()))
      {
        ELLE_TRACE("create snapshot directoy %s", machine.transaction().snapshots_directory());
        boost::filesystem::create_directories(machine.transaction().snapshots_directory());
      }
      return machine.transaction().snapshots_directory() / "fsm.snapshot";
    }

    void
    TransactionMachine::Snapshot::print(std::ostream& stream) const
    {
      elle::fprintf(stream, "TransactionMachine::Snapshot(\"%s\")",
                    this->_current_state);
    }

    TransactionMachine::Snapshot
    TransactionMachine::snapshot() const
    {
      boost::filesystem::path path = Snapshot::path(*this);
      if (!exists(path))
        throw elle::Error(elle::sprintf("missing snapshot: %s", path));
      elle::AtomicFile source(path);
      return source.read() << [&] (elle::AtomicFile::Read& read)
      {
        elle::serialization::json::SerializerIn input(read.stream(), false);
        return Snapshot(input);
      };
    }

    /*-------------------.
    | TransactionMachine |
    `-------------------*/

    TransactionMachine::TransactionMachine(
      Transaction& transaction,
      uint32_t id,
      std::shared_ptr<TransactionMachine::Data> data)
      : _id(id)
      , _machine(elle::sprintf("transaction (%s) fsm", id))
      , _machine_thread()
      , _another_device_state(
        this->_machine.state_make(
          "another device",
          std::bind(&TransactionMachine::_another_device, this)))
      , _cancel_state(
        this->_machine.state_make(
          "cancel", std::bind(&TransactionMachine::_cancel, this)))
      , _end_state(
        this->_machine.state_make(
          "end", std::bind(&TransactionMachine::_end, this)))
      , _fail_state(
        this->_machine.state_make(
          "fail", std::bind(&TransactionMachine::_fail, this)))
      , _finish_state
        (this->_machine.state_make("finish", [this] {this->_finish(); }))
      , _reject_state(
        this->_machine.state_make(
          "reject", std::bind(&TransactionMachine::_reject, this)))
      , _transfer_state(
        this->_machine.state_make(
          "transfer", std::bind(&TransactionMachine::_transfer, this)))
      , _finished("finished")
      , _rejected("rejected")
      , _canceled("canceled")
      , _failed("failed")
      , _transaction(transaction)
      , _state(transaction.state())
      , _data(std::move(data))
    {
      ELLE_TRACE_SCOPE("%s: create transaction machine", *this);
      // Transfer end.
      this->_machine.transition_add(
        this->_transfer_state,
        this->_end_state,
        reactor::Waitables{&this->finished()},
        true);
      this->_machine.transition_add(
        this->_transfer_state,
        this->_cancel_state,
        reactor::Waitables{&this->canceled()},
        true);
      this->_machine.transition_add(
        this->_transfer_state,
        this->_fail_state,
        reactor::Waitables{&this->failed()},
        true);
      this->_fail_on_exception(this->_transfer_state);
      // Another device endings.
      this->_machine.transition_add(
        this->_another_device_state,
        this->_end_state,
        reactor::Waitables{&this->finished()});
      this->_machine.transition_add(
        this->_another_device_state,
        this->_cancel_state,
        reactor::Waitables{&this->canceled()});
      this->_machine.transition_add(
        this->_another_device_state,
        this->_end_state,
        reactor::Waitables{&this->failed()});
      // End.
      this->_machine.transition_add(this->_finish_state, this->_end_state);
      this->_machine.transition_add(this->_cancel_state, this->_end_state);
      this->_machine.transition_add(this->_fail_state, this->_end_state);
      // Reject.
      this->_machine.transition_add(this->_reject_state, this->_end_state);
      // The catch transitions just open the barrier for logging purpose.
      // The snapshot will be kept.
      this->_machine.transition_add_catch(this->_fail_state, this->_end_state)
        .action([this] { ELLE_ERR("%s: failure failed", *this); });
      this->_machine.transition_add_catch(this->_cancel_state, this->_end_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_ERR("%s: cancellation failed: %s",
                     *this, elle::exception_string(e));
            this->transaction().failure_reason(elle::exception_string(e));
            this->_failed.open();
          });
      this->_machine.transition_add_catch(this->_finish_state, this->_end_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_ERR("%s: termination failed: %s",
                     *this, elle::exception_string(e));
            this->transaction().failure_reason(elle::exception_string(e));
            this->_failed.open();
          });
      this->_machine.transition_triggered().connect(
        [this] (reactor::fsm::Transition& transition)
        {
          ELLE_LOG_COMPONENT("surface.gap.TransactionMachine.Transition");
          ELLE_TRACE("%s: %s triggered", *this, transition);
        });
      this->_machine.state_changed().connect(
        [this] (reactor::fsm::State const& state)
        {
          this->_save_snapshot();
          ELLE_LOG_COMPONENT("surface.gap.TransactionMachine.State");
          ELLE_TRACE("%s: entering %s", *this, state);
        });
    }

    TransactionMachine::~TransactionMachine()
    {
      ELLE_TRACE_SCOPE("%s: destroying transaction machine", *this);
    }

    void
    TransactionMachine::_fail_on_exception(reactor::fsm::State& state)
    {
      this->_machine.transition_add_catch(
        state,
        this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_WARN("%s: fatal error: %s",
                      *this, elle::exception_string(e));
            this->transaction().failure_reason(elle::exception_string(e));
          });
    }

    void
    TransactionMachine::_save_snapshot() const
    {
      boost::filesystem::path path = Snapshot::path(*this);
      ELLE_TRACE_SCOPE("%s: save snapshot to %s", *this, path);
      elle::AtomicFile destination(path);
      destination.write() << [&] (elle::AtomicFile::Write& write)
      {
        elle::serialization::json::SerializerOut output(write.stream(), false);
        Snapshot(*this).serialize(output);
      };
    }

    void
    TransactionMachine::gap_status(gap_TransactionStatus v)
    {
      this->_transaction.status(v);
    }

    void
    TransactionMachine::_another_device()
    {
      ELLE_TRACE("%s: transaction running on another device", *this);
      this->gap_status(gap_transaction_on_other_device);
    }

    void
    TransactionMachine::_end()
    {
      ELLE_TRACE_SCOPE("%s: end", *this);
      auto status = this->data()->status;
      switch (status)
      {
        case infinit::oracles::Transaction::Status::finished:
        case infinit::oracles::Transaction::Status::ghost_uploaded:
          this->gap_status(gap_transaction_finished);
          break;
        case infinit::oracles::Transaction::Status::rejected:
          this->gap_status(gap_transaction_rejected);
          break;
        case infinit::oracles::Transaction::Status::canceled:
          this->gap_status(gap_transaction_canceled);
          break;
        case infinit::oracles::Transaction::Status::failed:
          this->gap_status(gap_transaction_failed);
          break;
        case infinit::oracles::Transaction::Status::accepted:
        case infinit::oracles::Transaction::Status::cloud_buffered:
        case infinit::oracles::Transaction::Status::created:
        case infinit::oracles::Transaction::Status::deleted:
        case infinit::oracles::Transaction::Status::initialized:
        case infinit::oracles::Transaction::Status::none:
        case infinit::oracles::Transaction::Status::started:
          ELLE_ERR("%s: impossible final status: %s", *this, status);
          this->gap_status(gap_transaction_failed);
          break;
      }
      this->_transaction._over = true;
      this->cleanup();
      boost::system::error_code error;
      auto path = this->transaction().snapshots_directory();
      boost::filesystem::remove_all(path, error);
      if (error)
        ELLE_WARN("%s: unable to remove snapshot directory %s: %s",
                  *this, path, error.message());
    }

    void
    TransactionMachine::_finish()
    {
      this->_finish(infinit::oracles::Transaction::Status::finished);
    }

    void
    TransactionMachine::_finish(infinit::oracles::Transaction::Status status)
    {
      ELLE_TRACE_SCOPE("%s: finish", *this);
      this->_finalize(status);
    }

    void
    TransactionMachine::_reject()
    {
      ELLE_TRACE_SCOPE("%s: reject", *this);
      this->_finalize(infinit::oracles::Transaction::Status::rejected);
    }

    void
    TransactionMachine::_cancel()
    {
      ELLE_TRACE_SCOPE("%s: cancel", *this);
      this->_finalize(infinit::oracles::Transaction::Status::canceled);
    }

    void
    TransactionMachine::_fail()
    {
      ELLE_TRACE_SCOPE("%s: fail", *this);
      this->_metrics_ended(infinit::oracles::Transaction::Status::failed,
                           transaction().failure_reason());
      std::string transaction_id;
      if (!this->data()->id.empty())
        transaction_id = this->transaction_id();
      else
        transaction_id = "unknown";
      try
      {
        // Send report for failed transfer
        auto transaction_dir =
          common::infinit::transactions_directory(this->state().home(),
                                                  this->state().me().id);
        elle::crash::transfer_failed_report(this->state().meta().protocol(),
                                            this->state().meta().host(),
                                            this->state().meta().port(),
                                            transaction_dir,
                                            this->state().me().email,
                                            transaction_id,
                                            this->transaction().failure_reason());
      }
      catch (elle::Error const& e)
      {
        ELLE_ERR("unable to report transaction failure: %s", e);
      }
      this->_finalize(infinit::oracles::Transaction::Status::failed);
    }

    void
    TransactionMachine::_run(reactor::fsm::State& initial_state)
    {
      ELLE_TRACE_SCOPE("%s: start transaction machine at %s",
                       *this, initial_state);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      this->_machine_thread.reset(
        new reactor::Thread{
          "TransactionMachine::run",
          [&]
          {
            try
            {
              this->_machine.run(initial_state);
              ELLE_TRACE("%s: machine finished properly", *this);
            }
            catch (elle::Error const&)
            {
              // FIXME: this should be a hard failure. Exception should never
              // escape the machine, cancelling the transaction is not enough:
              // the snapshots, mirrors, etc. won't be cleaned.
              ELLE_WARN("%s: Exception escaped fsm run: %s",
                        *this, elle::exception_string());
              // Pretend this did not happen if state is final, or cancel.
              if (!_transaction.final())
              {
                try
                {
                  _transaction.cancel();
                }
                catch (elle::Error const&)
                {
                  // transaction can be in a non-cancelleable state (not
                  // initialized)
                }
              }
            }
          }});
    }

    void
    TransactionMachine::peer_available(
      std::vector<std::pair<std::string, int>> const& local_endpoints,
      std::vector<std::pair<std::string, int>> const& public_endpoints)
    {}

    void
    TransactionMachine::peer_unavailable()
    {}

    void
    TransactionMachine::notify_user_connection_status(std::string const&,
                                                      bool,
                                                      std::string const&,
                                                      bool)
    {}

    void
    TransactionMachine::cancel(std::string const& reason)
    {
      ELLE_TRACE_SCOPE("%s: cancel transaction %s", *this, this->data()->id);
      if (!this->canceled().opened())
        this->_metrics_ended(infinit::oracles::Transaction::Status::canceled,
                             reason);
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

    void
    TransactionMachine::_finalize(infinit::oracles::Transaction::Status s)
    {
      ELLE_TRACE_SCOPE("%s: finalize transaction: %s", *this, s);
      if (this->data()->id.empty())
        ELLE_TRACE("%s: no need to finalize transaction: id is still empty",
                   *this);
      else
      {
        try
        {
          this->_update_meta_status(s);
          this->data()->status = s;
          this->transaction()._snapshot_save();
          this->_metrics_ended(s);
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
        catch (elle::Error const&)
        {
          ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                   *this, this->transaction_id(), elle::exception_string());
        }
      }
    }

    /*-----------.
    | Attributes |
    `-----------*/

    std::string const&
    TransactionMachine::transaction_id() const
    {
      if (this->_data->id.empty())
        throw elle::Error(elle::sprintf("%s: not ready", *this));
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
      elle::SafeFinally signal{[&]
        {
          this->_transaction_id_set(this->_data->id);
        }};
      this->_data->id = id;
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

    void
    TransactionMachine::reset_transfer()
    {
      this->reset_transfer_signal().signal();
    }

    void
    TransactionMachine::_report_s3_error(aws::AWSException const& exception, bool will_retry)
    {
      unsigned attempt = exception.attempt();
      // Avoid flooding the metrics, report every tenth above 10, every hundred
      // above 100, ...
      if (attempt > 10 && attempt % (unsigned)pow(10, (unsigned)log10(attempt)))
        return;
      if (auto& mr = state().metrics_reporter())
      {
        int http_status = 0;
        std::string aws_error_code;
        std::string message = exception.what();
        if (exception.inner_exception())
        {
          if (auto awserror = dynamic_cast<aws::RequestError*>(
            exception.inner_exception().get()))
          {
            if (auto& ec = awserror->error_code())
              aws_error_code = *ec;
            if (auto& hs = awserror->http_status())
              http_status = static_cast<int>(*hs);
            message = awserror->what();
          }
          else
            message = exception.inner_exception()->what();
        }
        mr->aws_error(this->transaction_id(),
                      exception.operation(),
                      exception.url(),
                      exception.attempt(),
                      http_status,
                      aws_error_code,
                      (will_retry? "TRANSIENT:":"FATAL:") + message);
      }
    }

    /*--------.
    | Metrics |
    `--------*/

    void
    TransactionMachine::_metrics_ended(
      infinit::oracles::Transaction::Status status,
      std::string reason)
    {
      bool onboarding = false;
      if (this->state().metrics_reporter())
        this->state().metrics_reporter()->transaction_ended(
          this->transaction_id(),
          status,
          reason,
          onboarding,
          this->transaction().canceled_by_user());
    }

    /*---.
    | S3 |
    `---*/

    S3::S3(State& state,
           std::function<aws::Credentials(bool)> query_credentials)
      : aws::S3(query_credentials)
      , _state(state)
    {}

    aws::URL
    S3::hostname(aws::Credentials const& credentials) const
    {
      auto replace = this->_state.s3_hostname();
      if (!replace)
        return aws::S3::hostname(credentials);
      else
        return *replace;
    }

  }
}
