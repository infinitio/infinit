#include <surface/gap/LinkSendMachine.hh>

#include <elle/Error.hh>
#include <elle/container/vector.hh>
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
          if (this->data()->id.empty())
            this->_run(this->_create_transaction_state);
          else
            this->_run(this->_upload_state);
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
          this->_run(this->_reject_state);
          break;
        case TransactionStatus::accepted:
        case TransactionStatus::none:
        case TransactionStatus::started:
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
    LinkSendMachine::transaction_status_update(
      infinit::oracles::Transaction::Status status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);
    }

    aws::Credentials
    LinkSendMachine::_aws_credentials(bool first_time)
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
      return this->_credentials.get();
    }

    void
    LinkSendMachine::_create_transaction()
    {
      infinit::oracles::LinkTransaction::FileList files;
      // FIXME: handle directories, non existing files, empty list and shit.
      for (auto const& file: this->files())
      {
        boost::filesystem::path path(file);
        files.emplace_back(path.filename().string(),
                           elle::os::file::size(path.string()));
      }
      auto response =
        this->state().meta().create_link(
          files, this->archive_info().first, this->message());
      *this->_data = std::move(response.transaction());
      this->transaction()._snapshot_save();
      this->_credentials = std::move(response.aws_credentials());
      this->_save_snapshot();
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
      }
      catch (elle::Exception const&)
      {
        ELLE_ERR("%s: unable to finalize the transaction %s: %s",
                 *this, this->transaction_id(), elle::exception_string());
      }
    }
  }
}
