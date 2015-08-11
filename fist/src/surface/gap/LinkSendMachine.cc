#include <surface/gap/LinkSendMachine.hh>

#include <elle/Error.hh>
#include <elle/os/file.hh>

#include <infinit/oracles/meta/error/AccountLimitationError.hh>

#include <surface/gap/State.hh>
#include <surface/gap/Transaction.hh>

ELLE_LOG_COMPONENT("surface.gap.LinkSendMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = infinit::oracles::Transaction::Status;

    /*-------------.
    | Construction |
    `-------------*/

    LinkSendMachine::LinkSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::shared_ptr<Data> data,
                                     bool run_to_fail)
      : Super::Super(transaction, id, data)
      , Super(transaction, id, data)
      , _message(data->message)
      , _screenshot(data->screenshot ? data->screenshot.get() : false)
      , _data(data)
      , _completed(false)
    {
      if (run_to_fail)
        this->_run(this->_fail_state);
      else
      {
        this->_run(this->_another_device_state);
        this->transaction_status_update(data->status);
      }
    }

    LinkSendMachine::LinkSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::vector<std::string> files,
                                     std::string const& message,
                                     std::shared_ptr<Data> data)
      : Super::Super(transaction, id, data)
      , Super(transaction, id, std::move(files), data)
      , _message(message)
      , _screenshot(data->screenshot ? data->screenshot.get() : false)
      , _data(data)
      , _credentials()
      , _completed(false)
    {
      this->_machine.transition_add(
        this->_initialize_transaction_state,
        this->_transfer_state);
      this->_machine.transition_add(
        this->_transfer_state,
        this->_finish_state);
      this->_machine.transition_add(
        this->_unpausing_state,
        this->_transfer_state,
        reactor::Waitables{&!this->transaction().paused()});
      if (data->id.empty())
        // Transaction has just been created locally.
        this->_run(this->_create_transaction_state);
      else
        this->_run_from_snapshot();

      typedef infinit::oracles::meta::LinkQuotaExceeded QuotaExceeded;
      auto quota_exceeded_e = [this] (std::exception_ptr e)
      {
        try
        {
          std::rethrow_exception(e);
        }
        catch (QuotaExceeded const& e)
        {
          ELLE_WARN("link quota exceeded: %s", e);
          gap_Status meta_error = static_cast<gap_Status>(e.meta_error());
          this->gap_status(gap_transaction_payment_required, meta_error);
          if (this->state().metrics_reporter())
            this->state().metrics_reporter()->link_quota_exceeded(
              this->total_size(), e.quota(), e.usage());
          return;
        }
        elle::unreachable();
      };

      this->_machine.transition_add_catch_specific<QuotaExceeded>(
        this->_create_transaction_state, this->_cancel_state, true).action_exception(
          quota_exceeded_e);
      this->_machine.transition_add_catch_specific<QuotaExceeded>(
        this->_initialize_transaction_state, this->_cancel_state, true).action_exception(
          quota_exceeded_e);
    }

    void
    LinkSendMachine::_run_from_snapshot()
    {
      bool started = false;
      try
      {
        auto snapshot = this->snapshot();
        ELLE_TRACE_SCOPE("%s: restore from snapshot", *this);
        started = true;
        if (snapshot.current_state() == "cancel")
          this->_run(this->_cancel_state);
        else if (snapshot.current_state() == "create transaction")
          this->_run(this->_create_transaction_state);
        else if (snapshot.current_state() == "initialize transaction")
            this->_run(this->_initialize_transaction_state);
        else if (snapshot.current_state() == "end")
          this->_run(this->_end_state);
        else if (snapshot.current_state() == "fail")
          this->_run(this->_fail_state);
        else if (snapshot.current_state() == "finish")
          this->_run(this->_finish_state);
        else if (snapshot.current_state() == "reject")
          this->_run(this->_reject_state);
        else if (snapshot.current_state() == "upload")
          this->_run(this->_transfer_state);
        else if (snapshot.current_state() == "another device")
          this->_run(this->_another_device_state);
        else
        {
          ELLE_WARN("%s: unknown state in snapshot: %s",
                    *this, snapshot.current_state());
          started = false;
        }
      }
      catch (elle::Error const&)
      {
        ELLE_WARN("%s: can't load snapshot: %s",
                  *this, elle::exception_string());
      }
      if (started)
        return;
      // Try to guess a decent starting state from the transaction status.
      ELLE_TRACE_SCOPE(
        "%s: deduce starting state from the transaction status: %s",
        *this, this->data()->status);
      switch (this->data()->status)
      {
        case TransactionStatus::initialized:
        case TransactionStatus::created:
          if (this->concerns_this_device())
          {
            if (this->data()->id.empty())
              this->_run(this->_create_transaction_state);
            else
              this->_run(this->_transfer_state);
          }
          else
          {
            this->_run(this->_another_device_state);
          }
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
        case TransactionStatus::cloud_buffered:
        case TransactionStatus::ghost_uploaded:
        case TransactionStatus::none:
        case TransactionStatus::started:
        case TransactionStatus::accepted:
        case TransactionStatus::rejected:
        case TransactionStatus::deleted:
          elle::unreachable();
      }
    }

    LinkSendMachine::~LinkSendMachine()
    {
      this->_stop();
    }

    /*---------------.
    | Implementation |
    `---------------*/

    void
    LinkSendMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);
      switch (status)
      {
        case TransactionStatus::initialized:
        case TransactionStatus::created:
          if (this->concerns_this_device())
            ELLE_TRACE("%s: ignoring status update to %s", *this, status);
          else
            this->gap_status(gap_transaction_on_other_device);
          break;
        case TransactionStatus::finished:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->finished().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_finished);
          break;
        case TransactionStatus::canceled:
          ELLE_DEBUG("%s: open canceled barrier", *this)
            this->canceled().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_canceled);
          break;
        case TransactionStatus::failed:
          if (this->transaction().failure_reason().empty())
            this->transaction().failure_reason("peer failure");
          ELLE_DEBUG("%s: open failed barrier", *this)
            this->failed().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_failed);
          break;
        case TransactionStatus::deleted:
          ELLE_DEBUG("%s: update to deleted", *this)
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_deleted);
          break;
        case TransactionStatus::cloud_buffered:
        case TransactionStatus::ghost_uploaded:
        case TransactionStatus::none:
        case TransactionStatus::started:
        case TransactionStatus::accepted:
        case TransactionStatus::rejected:
          elle::unreachable();
      }
    }

    std::unique_ptr<infinit::oracles::meta::CloudCredentials>
    LinkSendMachine::_cloud_credentials(bool first_time)
    {
      if (this->data()->id.empty())
        ELLE_ABORT("%s: fetching AWS credentials of uncreated transaction",
                   *this);
      if (!first_time)
        // Force credentials regeneration.
        this->_credentials =
          this->state().meta().link_credentials(this->data()->id, true);
      else if (!this->_credentials)
        this->_credentials =
          this->state().meta().link_credentials(this->data()->id);
      return std::unique_ptr<infinit::oracles::meta::CloudCredentials>(
        this->_credentials->clone());
    }

    bool
    LinkSendMachine::completed() const
    {
      return this->_completed;
    }

    void LinkSendMachine::_create_transaction()
    {
      auto link_id = this->state().meta().create_link();
      this->transaction_id(link_id);
      this->transaction()._snapshot_save();
    }

    void
    LinkSendMachine::_initialize_transaction()
    {
      infinit::oracles::LinkTransaction::FileList files;
      // FIXME: handle directories, non existing files, empty list and shit.
      int64_t total_size = 0;
      try
      {
        for (auto const& file: this->files())
        {
          boost::filesystem::path path(file);
          files.emplace_back(path.filename().string(),
                             elle::os::file::size(path.string()));
          total_size += elle::os::file::size(file);
        }
      }
      catch (boost::filesystem::filesystem_error const& e)
      {
        ELLE_LOG("%s: Error while scanning files: %s", *this, e.what());
        throw;
      }
      // Mirror files as soon as possible.
      if (!archive_info().second)
        try_mirroring_files(total_size);
      {
        auto lock = this->state().transaction_update_lock.lock();
        auto response =
          this->state().meta().fill_link(
            files, this->archive_info().first, this->message(),
            this->screenshot(), this->transaction_id());
        *this->_data = std::move(response.transaction());
        this->transaction()._snapshot_save();
        this->_credentials = std::move(response.cloud_credentials());
      }
      this->total_size(total_size);
      this->_save_snapshot();
      if (this->state().metrics_reporter())
      {
        auto extensions = this->get_extensions();
        this->state().metrics_reporter()->link_transaction_created(
          this->transaction_id(),
          this->state().me().id,
          this->files().size(),
          total_size,
          this->_message.length(),
          this->screenshot(),
          std::vector<std::string>(extensions.begin(), extensions.end()));
      }
    }

    void
    LinkSendMachine::_transfer()
    {
      this->gap_status(gap_transaction_transferring);
      this->_plain_upload();
      this->_completed = true;
    }

    void
    LinkSendMachine::_update_meta_status(
      infinit::oracles::Transaction::Status status)
    {
      this->state().meta().update_link(this->data()->id, boost::none, status);
    }

    void
    LinkSendMachine::_pause(bool pause_action)
    {
      this->state().meta().update_link(this->data()->id,
                                       boost::none, // progress
                                       boost::none, // status
                                       pause_action);
    }
  }
}
