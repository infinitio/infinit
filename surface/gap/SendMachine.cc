#include <surface/gap/SendMachine.hh>
#include <surface/gap/Rounds.hh>

#include <frete/Frete.hh>

#include <station/Station.hh>

#include <papier/Descriptor.hh>

#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Subject.hh>

#include <reactor/thread.hh>
#include <reactor/exception.hh>

#include <elle/os/path.hh>
#include <elle/os/file.hh>

#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("surface.gap.SendMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = plasma::TransactionStatus;
    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::shared_ptr<TransferMachine::Data> data,
                             bool):
      TransferMachine(state, id, std::move(data)),
      _create_transaction_state(
        this->_machine.state_make(
          "create transaction", std::bind(&SendMachine::_create_transaction, this))),
      _wait_for_accept_state(
        this->_machine.state_make(
          "wait for accept", std::bind(&SendMachine::_wait_for_accept, this))),
      _accepted("accepted barrier"),
      _rejected("rejected barrier")
    {
      this->_machine.transition_add(
        this->_create_transaction_state,
        this->_wait_for_accept_state);

      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_transfer_core_state,
        reactor::Waitables{&this->_accepted},
        false,
        [&] () -> bool
        {
          // Pre Trigger the condition if the accepted barrier has already been
          // opened.
          return this->state().transactions().at(this->id()).data()->status ==
            TransactionStatus::accepted;
        }
        );

      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_reject_state,
        reactor::Waitables{&this->_rejected},
        false,
        [&] () -> bool
        {
          // Pre Trigger the condition if the rejected barrier has already been
          // opened.
          return this->state().transactions().at(this->id()).data()->status ==
            TransactionStatus::rejected;
        }
        );

      // Cancel.
      this->_machine.transition_add(this->_create_transaction_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      this->_machine.transition_add(this->_wait_for_accept_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->_canceled}, true);
      // Exception.
      this->_machine.transition_add_catch(
        this->_create_transaction_state, this->_fail_state);
      this->_machine.transition_add_catch(
        this->_wait_for_accept_state, this->_fail_state);
    }

    SendMachine::~SendMachine()
    {
      this->_stop();
    }

    SendMachine::Snapshot
    SendMachine::_make_snapshot() const
    {
      return Snapshot{*this->data(), this->_current_state, this->_files, this->_message};
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::shared_ptr<TransferMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_WARN_SCOPE("%s: constructing machine for transaction data %s "
                      "(not found on local snapshots)",
                       *this, this->data());

      switch (this->data()->status)
      {
        case plasma::TransactionStatus::initialized:
          this->_run(this->_wait_for_accept_state);
          break;
        case plasma::TransactionStatus::accepted:
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

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::string const& recipient,
                             std::unordered_set<std::string>&& files,
                             std::string const& message,
                             std::shared_ptr<TransferMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: send %s to %s", *this, files, recipient);

      this->_message = message;

      if (files.empty())
        throw Exception(gap_no_file, "no files to send");

      std::swap(this->_files, files);

      ELLE_ASSERT_NEQ(this->_files.size(), 0u);

      // Copy filenames into data structure to be sent to meta.
      this->data()->files.resize(this->_files.size());
      std::transform(
        this->_files.begin(),
        this->_files.end(),
        this->data()->files.begin(),
        [] (std::string const& el)
        {
          return boost::filesystem::path(el).filename().string();
        });

      ELLE_ASSERT_EQ(this->data()->files.size(), this->_files.size());

      this->peer_id(recipient);
      this->_run(this->_create_transaction_state);
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::unordered_set<std::string> files,
                             TransferMachine::State current_state,
                             std::string const& message,
                             std::shared_ptr<TransferMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s, starting at %s",
                       *this, *this->data(), current_state);
      this->_files = std::move(files);

      ELLE_ASSERT_NEQ(this->_files.size(), 0u);

      // Copy filenames into data structure to be sent to meta.
      this->data()->files.resize(this->_files.size());
      std::transform(
        this->_files.begin(),
        this->_files.end(),
        this->data()->files.begin(),
        [] (std::string const& el)
        {

          return boost::filesystem::path(el).filename().string();
        });
      ELLE_ASSERT_EQ(this->data()->files.size(), this->_files.size());

      this->_current_state = current_state;
      this->_message = message;
      switch (current_state)
      {
        case TransferMachine::State::NewTransaction:
          elle::unreachable();
        case TransferMachine::State::SenderCreateTransaction:
          this->_run(this->_create_transaction_state);
          break;
        case TransferMachine::State::SenderWaitForDecision:
          this->_run(this->_wait_for_accept_state);
          break;
        case TransferMachine::State::RecipientWaitForDecision:
        case TransferMachine::State::RecipientAccepted:
          elle::unreachable();
        case TransferMachine::State::PublishInterfaces:
        case TransferMachine::State::Connect:
        case TransferMachine::State::PeerDisconnected:
        case TransferMachine::State::PeerConnectionLost:
        case TransferMachine::State::Transfer:
          this->_run(this->_transfer_core_state);
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
      this->current_state(current_state);
    }

    void
    SendMachine::transaction_status_update(plasma::TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
        case plasma::TransactionStatus::accepted:
          ELLE_DEBUG("%s: open accepted barrier", *this)
            this->_accepted.open();
          break;
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
        case plasma::TransactionStatus::rejected:
          ELLE_DEBUG("%s: open rejected barrier", *this)
            this->_rejected.open();
          break;
        case plasma::TransactionStatus::initialized:
          ELLE_DEBUG("%s: ignore status %s", *this, status);
          break;
        case plasma::TransactionStatus::created:
        case plasma::TransactionStatus::none:
        case plasma::TransactionStatus::started:
        case plasma::TransactionStatus::_count:
          elle::unreachable();
      }
    }

    void
    SendMachine::_create_transaction()
    {
      ELLE_TRACE_SCOPE("%s: create transaction", *this);
      this->current_state(TransferMachine::State::SenderCreateTransaction);

      auto total_size =
        [] (std::unordered_set<std::string> const& files) -> size_t
        {
          ELLE_TRACE_FUNCTION(files);

          size_t size = 0;
          {
            for (auto const& file: files)
            {
              auto _size = elle::os::file::size(file);
              ELLE_DEBUG("%s: %i", file, _size);
              size += _size;
            }
          }
          return size;
        };

      int size = total_size(this->_files);

      std::string first_file =
        boost::filesystem::path(*(this->_files.cbegin())).filename().string();

      std::list<std::string> file_list{this->_files.size()};
      std::transform(
        this->_files.begin(),
        this->_files.end(),
        file_list.begin(),
        [] (std::string const& el) {
          return boost::filesystem::path(el).filename().string();
        });
      ELLE_ASSERT_EQ(file_list.size(), this->_files.size());

      ELLE_DEBUG("create transaction");
      this->transaction_id(
        this->state().meta().create_transaction(
          this->peer_id(),
          this->data()->files,
          this->data()->files.size(),
          size,
          boost::filesystem::is_directory(first_file),
          this->state().device().id,
          this->_message
          ).created_transaction_id
        );

      // This is fake, we don't create peer to peer network.
      this->state().google_reporter()[this->state().me().id].store(
        "network.create.succeed");

      ELLE_TRACE("created transaction: %s", this->transaction_id());

      // XXX: Ensure recipient is an id.

      ELLE_DEBUG("store peer id");
      this->peer_id(this->state().user(this->peer_id(), true).id);
      ELLE_TRACE("peer id: %s", this->peer_id());

      this->state().mixpanel_reporter()[this->transaction_id()].store(
        "transaction.created",
        {
          {MKey::sender, this->state().me().id},
          {MKey::recipient, this->peer_id()},
          {MKey::file_count, std::to_string(this->data()->files.size())},
          {MKey::file_size, std::to_string(size / (1024 * 1024))}
        });

      this->state().meta().update_transaction(this->transaction_id(),
                                              plasma::TransactionStatus::initialized);
    }

    void
    SendMachine::_wait_for_accept()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to accept or reject", *this);
      this->current_state(TransferMachine::State::SenderWaitForDecision);

      // There are two ways to go to the next step:
      // - Checking local state, meaning that during the copy, we recieved an
      //   accepted, so we can directly go the next step.
      // - Waiting for the accepted notification.
    }

    void
    SendMachine::_transfer_operation()
    {
      ELLE_TRACE_SCOPE("%s: transfer operation", *this);

      elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        scope.run_background(
          elle::sprintf("frete get %s", this->id()),
          [this] ()
          {
            this->frete().run();
          });
        scope.run_background(
          elle::sprintf("progress %s", this->id()),
          [this] ()
          {
            while (true)
            {
              ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

              ELLE_DEBUG("start waiting")
                try
                {
                  reactor::Scheduler::scheduler()->current()->wait(
                    this->frete().progress_changed());
                }
                catch (...)
                {
                  ELLE_DEBUG("exception finish waiting");
                  throw;
                }

              this->progress(this->frete().progress());
            }
          });
        this->_finished.wait();
      };
    }

    frete::Frete&
    SendMachine::frete()
    {
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      if (this->_frete == nullptr)
      {
        reactor::Scheduler& sched = *reactor::Scheduler::scheduler();
        ELLE_ASSERT(this->_frete == nullptr);

        ELLE_DEBUG("create serializer");
        this->_serializer.reset(
          new infinit::protocol::Serializer(sched,
                                            this->_host->socket()));
        ELLE_DEBUG("create channels");
        this->_channels.reset(
          new infinit::protocol::ChanneledStream(sched, *this->_serializer));

        this->_frete.reset(new frete::Frete(*this->_channels));

        ELLE_TRACE_SCOPE("%s: init frete", *this);
        for (std::string const& file: this->_files)
          this->_frete->add(file);
        ELLE_DEBUG("frete successfully initialized");
      }

      ELLE_ASSERT(this->_frete != nullptr);
      return *this->_frete;
    }

    /*----------.
    | Printable |
    `----------*/

    std::string
    SendMachine::type() const
    {
      return "SendMachine";
    }

  }
}
