#include <surface/gap/LinkSendMachine.hh>

#include <elle/Error.hh>
#include <elle/os/file.hh>

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
      , _data(data)
      , _upload_state(
        this->_machine.state_make(
          "upload", std::bind(&LinkSendMachine::_upload, this)))
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
      , _data(data)
      , _upload_state(
        this->_machine.state_make(
          "upload", std::bind(&LinkSendMachine::_upload, this)))
      , _credentials()
    {
      this->_machine.transition_add(this->_create_transaction_state,
                                    this->_initialize_transaction_state);
      this->_machine.transition_add(this->_initialize_transaction_state,
                                    this->_upload_state);
      this->_machine.transition_add(
        this->_upload_state,
        this->_finish_state,
        reactor::Waitables{&this->finished()},
        true);
      this->_machine.transition_add(
        this->_upload_state,
        this->_cancel_state,
        reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add_catch(
        this->_upload_state,
        this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_WARN("%s: error while uploading: %s",
                      *this, elle::exception_string(e));
            this->transaction().failure_reason(elle::exception_string(e));
          });
      this->_machine.transition_add(
        this->_upload_state,
        this->_fail_state,
        reactor::Waitables{&this->failed()}, true);
      if (data->id.empty())
        // Transaction has just been created locally.
        this->_run(this->_create_transaction_state);
      else
        this->_run_from_snapshot();
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
          this->_run(this->_upload_state);
        else if (snapshot.current_state() == "another device")
          this->_run(this->_another_device_state);
        else
        {
          ELLE_WARN("%s: unkown state in snapshot: %s",
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
              this->_run(this->_upload_state);
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

    void LinkSendMachine::_create_transaction()
    {
      auto link_id = this->state().meta().create_link();
      this->transaction_id(link_id);
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
          this->state().meta().create_link(
            files, this->archive_info().first, this->message(),
            this->transaction_id());
        *this->_data = std::move(response.transaction());
        this->transaction()._snapshot_save();
        this->_credentials = std::move(response.cloud_credentials());
      }
      this->total_size(total_size);
      this->_save_snapshot();
      if (this->state().metrics_reporter())
        this->state().metrics_reporter()->link_transaction_created(
          this->transaction_id(),
          this->state().me().id,
          this->files().size(),
          total_size,
          this->_message.length());
    }

    void
    LinkSendMachine::_upload()
    {
      this->gap_status(gap_transaction_transferring);
      this->_plain_upload();
    }

    void
    LinkSendMachine::_finalize(infinit::oracles::Transaction::Status s)
    {
      ELLE_TRACE_SCOPE("%s: finalize transaction: %s", *this, s);
      if (this->data()->id.empty())
      {
        ELLE_WARN("%s: can't finalize not yet created transaction", *this);
        return;
      }
      try
      {
        this->state().meta().update_link(this->data()->id, 0, s);
        this->data()->status = s;
        this->transaction()._snapshot_save();
        if (this->state().metrics_reporter() && this->concerns_this_device())
        {
          bool onboarding = false;
          this->state().metrics_reporter()->transaction_ended(
            this->transaction_id(),
            s,
            s == infinit::oracles::Transaction::Status::failed?
              transaction().failure_reason() : "",
            onboarding,
            this->transaction().canceled_by_user());
        }
      }
      catch (elle::Exception const&)
      {
        ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                 *this, this->transaction_id(), elle::exception_string());
      }
    }
  }
}
