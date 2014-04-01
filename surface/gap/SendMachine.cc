#include <boost/filesystem.hpp>

#include <elle/os/path.hh>
#include <elle/os/file.hh>

#include <reactor/thread.hh>
#include <reactor/exception.hh>

#include <common/common.hh>

#include <frete/Frete.hh>

#include <station/Station.hh>

#include <surface/gap/SendMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.SendMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = infinit::oracles::Transaction::Status;
    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::shared_ptr<TransactionMachine::Data> data,
                             bool):
      TransactionMachine(state, id, std::move(data)),
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
          return this->state().transactions().at(this->id())->data()->status ==
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
          return this->state().transactions().at(this->id())->data()->status ==
            TransactionStatus::rejected;
        }
        );

      // Cancel.
      this->_machine.transition_add(this->_create_transaction_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(this->_wait_for_accept_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->canceled()}, true);
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
                             std::shared_ptr<TransactionMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_WARN_SCOPE("%s: constructing machine for transaction data %s "
                      "(not found on local snapshots)",
                       *this, this->data());

      switch (this->data()->status)
      {
        case TransactionStatus::initialized:
          this->_run(this->_wait_for_accept_state);
          break;
        case TransactionStatus::accepted:
          this->_run(this->_transfer_core_state);
          break;
        case TransactionStatus::finished:
          this->_run(this->_finish_state);
          break;
        case TransactionStatus::canceled:
          this->_run(this->_cancel_state);
          break;
        case TransactionStatus::failed:
          this->_run(this->_fail_state);
          break;
        case TransactionStatus::rejected:
        case TransactionStatus::created:
          break;
        case TransactionStatus::started:
        case TransactionStatus::none:
          elle::unreachable();
      }
    }

    SendMachine::SendMachine(surface::gap::State const& state,
                             uint32_t id,
                             std::string const& recipient,
                             std::unordered_set<std::string>&& files,
                             std::string const& message,
                             std::shared_ptr<TransactionMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct to send %s to %s",
                       *this, files, recipient);

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
                             TransactionMachine::State current_state,
                             std::string const& message,
                             std::shared_ptr<TransactionMachine::Data> data):
      SendMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct from snapshot starting at %s",
                       *this, current_state);
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
        case TransactionMachine::State::NewTransaction:
          elle::unreachable();
        case TransactionMachine::State::SenderCreateTransaction:
          this->_run(this->_create_transaction_state);
          break;
        case TransactionMachine::State::SenderWaitForDecision:
          this->_run(this->_wait_for_accept_state);
          break;
        case TransactionMachine::State::RecipientWaitForDecision:
        case TransactionMachine::State::RecipientAccepted:
          elle::unreachable();
        case TransactionMachine::State::PublishInterfaces:
        case TransactionMachine::State::Connect:
        case TransactionMachine::State::PeerDisconnected:
        case TransactionMachine::State::PeerConnectionLost:
        case TransactionMachine::State::Transfer:
          this->_run(this->_transfer_core_state);
          break;
        case TransactionMachine::State::Finished:
          this->_run(this->_finish_state);
          break;
        case TransactionMachine::State::Rejected:
          this->_run(this->_reject_state);
          break;
        case TransactionMachine::State::Canceled:
          this->_run(this->_cancel_state);
          break;
        case TransactionMachine::State::Failed:
          this->_run(this->_fail_state);
          break;
        default:
          elle::unreachable();
      }
      this->current_state(current_state);
    }

    void
    SendMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
        case TransactionStatus::accepted:
          ELLE_DEBUG("%s: open accepted barrier", *this)
            this->accepted().open();
          break;
        case TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this)
            this->canceled().open();
          break;
        case TransactionStatus::failed:
          ELLE_DEBUG("%s: open failed barrier", *this)
            this->failed().open();
          break;
        case TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->finished().open();
          break;
        case TransactionStatus::rejected:
          ELLE_DEBUG("%s: open rejected barrier", *this)
            this->rejected().open();
          break;
        case TransactionStatus::initialized:
          break;
        case TransactionStatus::created:
        case TransactionStatus::none:
        case TransactionStatus::started:
          elle::unreachable();
      }
    }

    void
    SendMachine::_create_transaction()
    {
      ELLE_TRACE_SCOPE("%s: create transaction", *this);
      int64_t size = 0;
      for (auto const& file: this->_files)
      {
        auto _size = elle::os::file::size(file);
        size += _size;
      }
      ELLE_DEBUG("%s: total file size: %s", *this, size);
      this->data()->total_size = size;

      auto first_file = boost::filesystem::path(*(this->_files.cbegin()));

      std::list<std::string> file_list{this->_files.size()};
      std::transform(
        this->_files.begin(),
        this->_files.end(),
        file_list.begin(),
        [] (std::string const& el) {
          return boost::filesystem::path(el).filename().string();
        });
      ELLE_ASSERT_EQ(file_list.size(), this->_files.size());

      // Change state to SenderCreateTransaction once we've calculated the file
      // size and have the file list.
      this->current_state(TransactionMachine::State::SenderCreateTransaction);
      this->transaction_id(
        this->state().meta().create_transaction(
          this->peer_id(),
          this->data()->files,
          this->data()->files.size(),
          this->data()->total_size,
          boost::filesystem::is_directory(first_file),
          this->state().device().id,
          this->_message
          ).created_transaction_id
        );

      ELLE_TRACE("%s: created transaction %s", *this, this->transaction_id());

      // XXX: Ensure recipient is an id.
      this->peer_id(this->state().user(this->peer_id(), true).id);

      if (this->state().metrics_reporter())
        this->state().metrics_reporter()->transaction_created(
          this->transaction_id(),
          this->state().me().id,
          this->peer_id(),
          this->data()->files.size(),
          size,
          this->_message.length()
          );

      this->state().meta().update_transaction(this->transaction_id(),
                                              TransactionStatus::initialized);
    }

    void
    SendMachine::_wait_for_accept()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to accept or reject", *this);
      this->current_state(TransactionMachine::State::SenderWaitForDecision);

      // There are two ways to go to the next step:
      // - Checking local state, meaning that during the copy, we recieved an
      //   accepted, so we can directly go the next step.
      // - Waiting for the accepted notification.
    }

    void
    SendMachine::_transfer_operation(frete::RPCFrete& frete)
    {
      ELLE_TRACE_SCOPE("%s: transfer operation", *this);
      elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        scope.run_background(
          elle::sprintf("frete get %s", this->id()),
          [&frete] ()
          {
            frete.run();
          });
        scope.wait();
      };
    }

    float
    SendMachine::progress() const
    {
      if (this->_frete != nullptr)
        return this->_frete->progress();
      return 0.0f;
    }

    frete::Frete&
    SendMachine::frete()
    {
      if (this->_frete == nullptr)
      {
        infinit::cryptography::PublicKey peer_K;
        peer_K.Restore(this->state().user(this->peer_id(), true).public_key);

        this->_frete = elle::make_unique<frete::Frete>(
          this->transaction_id(),
          peer_K,
          common::infinit::frete_snapshot_path(
            this->data()->sender_id,
            this->data()->id));

        ELLE_TRACE("%s: initialize frete", *this)
          for (std::string const& file: this->_files)
            this->_frete->add(file);
      }

      return *this->_frete;
    }

    std::unique_ptr<frete::RPCFrete>
    SendMachine::rpcs(infinit::protocol::ChanneledStream& channels)
    {
      return elle::make_unique<frete::RPCFrete>(this->frete(), channels);
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
