#include <surface/gap/Error.hh>
#include <surface/gap/PeerSendMachine.hh>

#include <cryptography/PublicKey.hh>

#include <elle/Error.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <elle/os/file.hh>
#include <elle/serialization/json.hh>

#include <frete/RPCFrete.hh>
#include <frete/Frete.hh>
#include <frete/TransferSnapshot.hh>
#include <infinit/metrics/Reporter.hh>
#include <infinit/oracles/Transaction.hh>
#include <papier/Identity.hh>
#include <surface/gap/FilesystemTransferBufferer.hh>
#include <surface/gap/S3TransferBufferer.hh>
#include <surface/gap/State.hh>

ELLE_LOG_COMPONENT("surface.gap.PeerSendMachine")

namespace surface
{
  namespace gap
  {
    using TransactionStatus = infinit::oracles::Transaction::Status;

    /*-------------.
    | Construction |
    `-------------*/

    /// Constructor when sending from another device or if you have no
    /// snapshot as sender. In that case, run_to_fail is set to true.
    PeerSendMachine::PeerSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::shared_ptr<Data> data,
                                     bool run_to_fail)
      : TransactionMachine(transaction, id, data)
      , SendMachine(transaction, id, data)
      , PeerMachine(transaction, id, data)
      , _message(data->message)
      , _wait_for_accept_state(
        this->_machine.state_make(
          "wait for accept",
          std::bind(&PeerSendMachine::_wait_for_accept, this)))
    {
      if (run_to_fail)
        ELLE_TRACE_SCOPE("%s: run to fail", *this);
      else
        ELLE_TRACE_SCOPE("%s: on another device", *this);
      this->_machine.transition_add(
        this->_another_device_state,
        this->_end_state,
        reactor::Waitables{&this->rejected()});
      if (run_to_fail)
        this->_run(this->_fail_state);
      else
      {
        this->_run(this->_another_device_state);
        this->transaction_status_update(data->status);
      }
    }

    PeerSendMachine::PeerSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::string recipient,
                                     std::vector<std::string> files,
                                     std::string message,
                                     std::shared_ptr<Data> data,
                                     bool)
      : TransactionMachine(transaction, id, data)
      , SendMachine(transaction, id, std::move(files), data)
      , PeerMachine(transaction, id, data)
      , _message(std::move(message))
      , _recipient(std::move(recipient))
      , _accepted("accepted")
      , _rejected("rejected")
      , _ghost_uploaded("rejected")
      , _frete()
      , _wait_for_accept_state(
        this->_machine.state_make(
          "wait for accept",
          std::bind(&PeerSendMachine::_wait_for_accept, this)))
    {
      ELLE_TRACE_SCOPE("%s: generic peer send machine", *this);
      this->_setup_end_state(this->_wait_for_accept_state);
      this->_machine.transition_add(
        this->_initialize_transaction_state,
        this->_wait_for_accept_state);
      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_finish_state,
        reactor::Waitables{&this->ghost_uploaded()},
        true);
      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_transfer_state,
        reactor::Waitables{&this->accepted()},
        true);
      this->_machine.transition_add(
        this->_wait_for_accept_state,
        this->_reject_state,
        reactor::Waitables{&this->rejected()},
        true);
      this->transaction_status_update(data->status);
    }

    /// Construct to send files.
    PeerSendMachine::PeerSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::string recipient,
                                     std::vector<std::string> files,
                                     std::string message,
                                     std::shared_ptr<Data> data)
      : PeerSendMachine(transaction,
                        id,
                        std::move(recipient),
                        std::move(files),
                        std::move(message),
                        std::move(data),
                        true)
    {
      ELLE_TRACE_SCOPE("%s: construct to send %s to %s",
                       *this, this->files(), this->recipient());
      // Copy filenames into data structure to be sent to meta.
      this->data()->files.resize(this->files().size());
      std::transform(
        this->files().begin(),
        this->files().end(),
        this->data()->files.begin(),
        [] (std::string const& el)
        {
          return boost::filesystem::path(el).filename().string();
        });
      this->data()->is_directory = boost::filesystem::is_directory(
        *this->files().begin());
      ELLE_ASSERT_EQ(this->data()->files.size(), this->files().size());
      this->_run(this->_create_transaction_state);
    }

    /// Construct from snapshot.
    PeerSendMachine::PeerSendMachine(Transaction& transaction,
                                     uint32_t id,
                                     std::vector<std::string> files,
                                     std::string message,
                                     std::shared_ptr<Data> data)
      : PeerSendMachine(transaction,
                        id,
                        data->recipient_id,
                        std::move(files),
                        std::move(message),
                        data,
                        true)
    {
      ELLE_TRACE_SCOPE("%s: construct from snapshot", *this);
      // Copy filenames into data structure to be sent to meta.
      this->data()->files.resize(this->files().size());
      std::transform(
        this->files().begin(),
        this->files().end(),
        this->data()->files.begin(),
        [] (std::string const& el)
        {
          return boost::filesystem::path(el).filename().string();
        });
      ELLE_ASSERT_EQ(this->data()->files.size(), this->files().size());
      this->_run_from_snapshot();
    }

    PeerSendMachine::~PeerSendMachine()
    {
      this->_stop();
    }

    void
    PeerSendMachine::_run_from_snapshot()
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
        else if (snapshot.current_state() == "transfer")
          this->_run(this->_transfer_state);
        else if (snapshot.current_state() == "wait for accept")
          this->_run(this->_wait_for_accept_state);
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
      // Try to guess a decent starting state from the transaction status.
      if (started)
        return;
      ELLE_TRACE_SCOPE(
        "%s: deduce starting state from the transaction status: %s",
        *this, this->data()->status);
      switch (this->data()->status)
      {
        case TransactionStatus::initialized:
        case TransactionStatus::created:
        case TransactionStatus::cloud_buffered:
          if (this->concerns_this_device())
          {
            if (this->data()->id.empty())
              this->_run(this->_create_transaction_state);
            else
              this->_run(this->_wait_for_accept_state);
          }
          else
          {
            this->_run(this->_another_device_state);
          }
          break;
        case TransactionStatus::accepted:
          if (this->concerns_this_device())
            this->_run(this->_transfer_state);
          break;
        case TransactionStatus::finished:
        case TransactionStatus::ghost_uploaded:
          this->_run(this->_finish_state);
          break;
        case TransactionStatus::canceled:
          this->_run(this->_cancel_state);
          break;
        case TransactionStatus::failed:
          this->_run(this->_fail_state);
          break;
        case TransactionStatus::rejected:
          this->rejected().open();
          break;
        case TransactionStatus::started:
        case TransactionStatus::none:
        case TransactionStatus::deleted:
          elle::unreachable();
      }
    }

    bool
    PeerSendMachine::completed() const
    {
      return this->_frete ? this->_frete->finished() : false;
    }

    /*---------------.
    | Implementation |
    `---------------*/

    void
    PeerSendMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);
      switch (status)
      {
        case TransactionStatus::initialized:
        case TransactionStatus::created:
        case TransactionStatus::cloud_buffered:
          if (this->concerns_this_device())
            ELLE_TRACE("%s: ignoring status update to %s", *this, status);
          else
            this->gap_status(gap_transaction_on_other_device);
          break;
        case TransactionStatus::accepted:
          if (this->concerns_this_device())
          {
            ELLE_DEBUG("%s: open accepted barrier", *this)
              this->accepted().open();
          }
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
        case TransactionStatus::finished:
        case TransactionStatus::ghost_uploaded:
          ELLE_DEBUG("%s: open finished barrier", *this)
            this->finished().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_finished);
          break;
        case TransactionStatus::rejected:
          ELLE_DEBUG("%s: open rejected barrier", *this)
            this->rejected().open();
          if (!this->concerns_this_device())
            this->gap_status(gap_transaction_rejected);
          break;
        case TransactionStatus::none:
        case TransactionStatus::started:
        case TransactionStatus::deleted:
          ELLE_ABORT("%s: invalid status update to %s", *this, status);
      }
    }

    typedef std::pair<frete::Frete::FileSize, frete::Frete::FileID> Position;
    static frete::Frete::FileSize
    progress_from(frete::Frete::FilesInfo const& infos, const Position& p)
    {
      frete::Frete::FileSize result = 0;
      for (unsigned i=0; i<p.first; ++i)
        result += infos[i].second;
      result += p.second;
      return result;
    }
    static std::streamsize const chunk_size = 1 << 18;

    void
    PeerSendMachine::_create_transaction()
    {
      ELLE_TRACE("%s: create transaction", *this);
      std::list<std::string> file_list{this->files().size()};
      std::transform(
        this->files().begin(),
        this->files().end(),
        file_list.begin(),
        [] (std::string const& el) {
          return boost::filesystem::path(el).filename().string();
        });
      this->transaction_id(this->state().meta().create_transaction(
        this->data()->recipient_id,
        file_list,
        file_list.size(),
        this->_message));
      this->transaction()._snapshot_save();
    }

    void
    PeerSendMachine::_initialize_transaction()
    {
      this->gap_status(gap_transaction_new);
      ELLE_TRACE_SCOPE("%s: initialize transaction", *this);
      int64_t size = 0;
      try
      {
        for (auto const& file: this->files())
          size += elle::os::file::size(file);
      }
      catch (boost::filesystem::filesystem_error const& e)
      {
        ELLE_LOG("%s: Error while scanning files: %s", *this, e.what());
        throw;
      }
      ELLE_DEBUG("%s: total file size: %s", *this, size);
      // Try mirroring files as soon as possible.
      // Before we only mirrored if we knew that it wasn't a ghost. This took
      // two meta calls to verify (create + fetch user). By mirroring here,
      // there is a chance that the mirrored files will not be used.
      // Note: If the recipient is a ghost, the archive will be created from
      // mirrored files.
      this->try_mirroring_files(size);
      this->data()->total_size = size;
      this->total_size(size);
      auto first_file = boost::filesystem::path(*(this->files().cbegin()));
      std::list<std::string> file_list{this->files().size()};
      std::transform(
        this->files().begin(),
        this->files().end(),
        file_list.begin(),
        [] (std::string const& el) {
          return boost::filesystem::path(el).filename().string();
        });
      ELLE_ASSERT_EQ(file_list.size(), this->files().size());
      // Change state to SenderCreateTransaction once we've calculated the file
      // size and have the file list.
      ELLE_TRACE("%s: initialize transaction, first_file=%s, dir=%s",
                 *this, first_file,
                 boost::filesystem::is_directory(first_file));
      {
        auto lock = this->state().transaction_update_lock.lock();
        boost::optional<elle::UUID> recipient_device_id;
        if (!this->data()->recipient_device_id.is_nil())
          recipient_device_id = this->data()->recipient_device_id;
        auto transaction_response =
          this->state().meta().create_transaction(
            this->data()->recipient_id,
            this->data()->files,
            this->data()->files.size(),
            this->data()->total_size,
            boost::filesystem::is_directory(first_file),
            this->state().device().id,
            this->_message,
            this->transaction_id(),
            recipient_device_id
            );
        auto const& peer = this->state().user_sync(
          transaction_response.recipient());
        this->data()->is_ghost = peer.ghost();
        this->data()->recipient_id = peer.id;
        this->transaction_id(transaction_response.created_transaction_id());
        this->transaction()._snapshot_save();
      }
      ELLE_TRACE("%s: initialized transaction %s", *this, this->transaction_id());
      // Populate the frete.
      if (!this->data()->is_ghost)
        this->frete().save_snapshot();
      if (this->state().metrics_reporter())
      {
        bool onboarding = false;
        this->state().metrics_reporter()->peer_transaction_created(
          this->transaction_id(),
          this->state().me().id,
          this->data()->recipient_id,
          this->data()->files.size(),
          size,
          this->_message.length(),
          this->data()->is_ghost,
          onboarding);
      }
      this->transaction().data()->status = TransactionStatus::initialized;
    }

    void
    PeerSendMachine::_wait_for_accept()
    {
      ELLE_TRACE_SCOPE("%s: waiting for peer to accept or reject", *this);
      auto peer = this->state().user(this->data()->recipient_id);
      try
      {
        // Normal p2p case.
        bool peer_online = peer.online();
        // Send to self case.
        if (this->state().me().id == peer.id)
          peer_online = peer.online_excluding_device(this->state().device().id);
        if (this->data()->is_ghost)
        {
          this->_plain_upload();
          this->_ghost_uploaded.open();
        }
        else if (this->data()->status == TransactionStatus::cloud_buffered)
        {
          ELLE_TRACE("%s: cloud buffered, nothing to do", *this);
          this->gap_status(gap_transaction_cloud_buffered);
        }
        else if (!peer.ghost() && !peer_online)
          this->_cloud_operation();
        else if (!peer.ghost())
        {
          // Peer is online. Wait for a grace period, and then
          // preemptively buffer to the cloud
          this->gap_status(gap_transaction_waiting_accept);
          auto const& features = this->state().configuration().features;
          auto it = features.find("preemptive_buffering_delay");
          if (it != features.end())
          {
            int delay = std::stoi(it->second);
            if (delay < 0)
              ELLE_TRACE("%s: preemptive buffering disabled (negative value)", *this);
            else
            {
              ELLE_TRACE("%s: buffering in %s seconds", *this, delay);
              reactor::sleep(boost::posix_time::seconds(delay));
              ELLE_TRACE("%s: buffering now", *this);
              this->_cloud_operation();
            }
          }
          else
            ELLE_TRACE("%s: preemptive buffering disabled (no feature)", *this);
        }
        else
        {
          ELLE_TRACE("%s: peer is online, doing nothing until acceptance",
                     *this);
          this->gap_status(gap_transaction_waiting_accept);
        }
      }
      catch (infinit::state::TransactionFinalized const&)
      {
        // Nothing to do, some kind of transition should push us to another
        // final state.
        ELLE_TRACE(
          "%s: transfer machine was stopped because transaction was finalized",
          *this);
      }
    }

    void
    PeerSendMachine::_finish()
    {
      this->_finish(transaction().data()->is_ghost
                    ? infinit::oracles::Transaction::Status::ghost_uploaded
                    : infinit::oracles::Transaction::Status::finished);
    }

    void
    PeerSendMachine::_transfer_operation(frete::RPCFrete& frete)
    {
      auto start_time = boost::posix_time::microsec_clock::universal_time();
      _fetch_peer_key(true);
      // save snapshot to get correct filepaths
      this->_save_frete_snapshot();
      ELLE_TRACE_SCOPE("%s: transfer operation, resuming at %s",
                       *this, this->frete().progress());
      boost::optional<infinit::metrics::TransferExitReason> exit_reason;
      std::string exit_message;
      frete::Frete::FileSize total_bytes_transfered = 0;
      frete::Frete::FileSize total_size = this->frete().full_size();
      float initial_progress = this->frete().progress();
      // Canceling from within RPC error handler is troublesome, do it async.
      bool must_cancel = false;
      int attempt = _transfer_machine->attempt();
      bool skip_report = (attempt > 10 && attempt % (unsigned)pow(10, (unsigned)log10(attempt)));
      auto& mr = state().metrics_reporter();
      if (mr && !skip_report)
      {
        auto now = boost::posix_time::microsec_clock::universal_time();
        mr->transaction_transfer_begin(
          this->transaction_id(),
          infinit::metrics::TransferMethodP2P,
          float((now - start_time).total_milliseconds()) / 1000.0f,
          attempt);
      }
      elle::SafeFinally write_end_message([&,this]
        {
          auto& mr = state().metrics_reporter();
          if (mr && !skip_report)
          {
            ELLE_ASSERT_NEQ(exit_reason, boost::none); // all codepath set the boost::optional
            auto now = boost::posix_time::microsec_clock::universal_time();
            float duration =
              float((now - start_time).total_milliseconds()) / 1000.0f;
            mr->transaction_transfer_end(this->transaction_id(),
                                         infinit::metrics::TransferMethodP2P,
                                         duration,
                                         total_bytes_transfered,
                                         *exit_reason,
                                         exit_message,
                                         attempt);
          }
        });
      // Handler for exceptions thrown by registered RPC procedures
      infinit::protocol::ExceptionHandler exception_handler =
      [&](std::exception_ptr ptr)
      {
        ELLE_TRACE("%s: RPC exception handler called with %s",
                   *this, elle::exception_string());
        try
        {
          std::rethrow_exception(ptr);
        }
        catch(boost::filesystem::filesystem_error const& bfs)
        {
          ELLE_WARN("%s: filesystem error '%s', canceling transaction",
                    *this,
                    elle::exception_string())
          total_bytes_transfered =
            total_size * (this->frete().progress() - initial_progress);
          exit_reason = infinit::metrics::TransferExitReasonError;
          exit_message = elle::exception_string();
          must_cancel = true;
          throw infinit::protocol::LastMessageException(exit_message);
        }
      };
      try
      {
        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          scope.run_background(
            elle::sprintf("frete get %s", this->id()),
            [&frete, &exception_handler] ()
            {
              frete.run(exception_handler);
            });
          scope.wait();
        };
        if (!exit_reason)
        {
          exit_reason = infinit::metrics::TransferExitReasonFinished;
          total_bytes_transfered =
            total_size * (this->frete().progress() - initial_progress);
        }
        if (must_cancel)
          this->canceled().open();
      }
      catch(reactor::Terminate const&)
      {
        total_bytes_transfered =
          total_size * (this->frete().progress() - initial_progress);
        exit_reason = infinit::metrics::TransferExitReasonTerminated;
        throw;
      }
      catch(...)
      { // Should not happen, RPC is eating everything.
        ELLE_ERR("%s: Unexpected exception '%s', cleaning and rethrowing...",
                  *this, elle::exception_string());
        total_bytes_transfered =
         total_size * (this->frete().progress() - initial_progress);
        exit_reason = infinit::metrics::TransferExitReasonError;
        exit_message = elle::exception_string();
        // Rethrow, exception will exit through fsm::run, let its caller decide
        // what to do.
        throw;
      }
      ELLE_TRACE("%s: exiting _transfer_operation normally", *this);
    }

    void
    PeerSendMachine::_cloud_operation()
    {
      if (!elle::os::getenv("INFINIT_NO_CLOUD_BUFFERING", "").empty())
      {
        ELLE_DEBUG("%s: cloud buffering disabled by configuration", *this);
        return;
      }
      ELLE_TRACE_SCOPE("%s: upload to the cloud", *this);
      this->gap_status(gap_transaction_transferring);
      auto start_time = boost::posix_time::microsec_clock::universal_time();
      infinit::metrics::TransferExitReason exit_reason = infinit::metrics::TransferExitReasonUnknown;
      std::string exit_message;
      uint64_t total_bytes_transfered = 0;
      elle::SafeFinally write_end_message([&,this]
        {
          if (auto& mr = state().metrics_reporter())
          {
            auto now = boost::posix_time::microsec_clock::universal_time();
            float duration =
              float((now - start_time).total_milliseconds()) / 1000.0f;
            mr->transaction_transfer_end(this->transaction_id(),
                                         infinit::metrics::TransferMethodCloud,
                                         duration,
                                         total_bytes_transfered,
                                         exit_reason,
                                         exit_message);
          }
        });
      try
      {
        auto& frete = this->frete();
        _fetch_peer_key(true);
        auto& snapshot = *frete.transfer_snapshot();
        // Save snapshot of what eventual direct upload already did right now.
        this->_save_frete_snapshot();
        FilesystemTransferBufferer::Files files;
        for (frete::Frete::FileID file_id = 0;
             file_id < snapshot.count();
             ++file_id)
        {
          auto& file = snapshot.file(file_id);
          files.push_back(std::make_pair(file.path(), file.size()));
        }
        bool cloud_debug =
          !elle::os::getenv("INFINIT_CLOUD_FILEBUFFERER", "").empty();
        std::unique_ptr<TransferBufferer> bufferer;
        if (cloud_debug)
        {
          bufferer.reset(
            new FilesystemTransferBufferer(*this->data(),
                                           "/tmp/infinit-buffering",
                                           snapshot.count(),
                                           snapshot.total_size(),
                                           files,
                                           frete.key_code()));
        }
        else
        {
          auto get_credentials = [this] (bool first_time)
            {
              auto creds = this->_cloud_credentials(first_time);
              auto awscreds = dynamic_cast<infinit::oracles::meta::CloudCredentialsAws*>(creds.get());
              ELLE_ASSERT(awscreds);
              return *static_cast<aws::Credentials*>(awscreds);
            };
          bufferer.reset(
            new S3TransferBufferer(
              elle::make_unique<S3>(this->state(), get_credentials),
              *this->data(),
              std::bind(&PeerSendMachine::_report_s3_error,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2),
              snapshot.count(),
              snapshot.total_size(),
              files,
              frete.key_code()));
        }
        if (auto& mr = state().metrics_reporter())
        {
          auto now = boost::posix_time::microsec_clock::universal_time();
          mr->transaction_transfer_begin(
            this->transaction_id(),
            infinit::metrics::TransferMethodCloud,
            float((now - start_time).total_milliseconds()) / 1000.0f
            );
        }
        /* Pipelined cloud upload with periodic local snapshot update
        */
        int num_threads = 8;
        std::string snum_threads =
          elle::os::getenv("INFINIT_NUM_CLOUD_UPLOAD_THREAD", "");
        if (!snum_threads.empty())
          num_threads = boost::lexical_cast<int>(snum_threads);
        typedef frete::Frete::FileSize FileSize;
        typedef frete::Frete::FileID FileID;
        FileSize transfer_since_snapshot = 0;
        FileID current_file = FileID(-1);
        FileSize current_position = 0;
        FileSize current_file_size = 0;
        // per-thread last pos
        std::vector<Position> last_acknowledge_block(
          num_threads, std::make_pair(0,0));
        FileSize acknowledge_position = 0;
        bool save_snapshot = false; // reqest one-shot snapshot save
        auto pipeline_cloud_upload = [&, this](int id)
        {
          while(true)
          {
            while (current_position >= current_file_size)
            {
              ++current_file;
              if (current_file >= snapshot.count())
                return;
              auto& file = snapshot.file(current_file);
              current_file_size = file.size();
              current_position = file.progress();
            }
            ELLE_DEBUG_SCOPE("%s: buffer file %s at offset %s/%s in the cloud",
                             *this, current_file, current_position, current_file_size);
            FileSize local_file = current_file;
            FileSize local_position = current_position;
            current_position += chunk_size;
            auto block = frete.encrypted_read_acknowledge(
              local_file, local_position, chunk_size, acknowledge_position);
            if (save_snapshot)
            {
              this->_save_frete_snapshot();
              save_snapshot = false;
            }
            auto& buffer = block.buffer();
            bufferer->put(local_file, local_position, buffer.size(), buffer);
            transfer_since_snapshot += buffer.size();
            total_bytes_transfered += buffer.size();
            last_acknowledge_block[id] = std::make_pair(local_file, local_position);
            if (transfer_since_snapshot >= 1000000)
            {
              // Update acknowledge position
              // First find the smallest value in per-thread last_ack
              // Since each thread is fetching blocks monotonically,
              // we know all blocks before that are acknowledged
              Position pmin = *std::min_element(
                last_acknowledge_block.begin(),
                last_acknowledge_block.end(),
                [](const Position& a, const Position&b)
                {
                  if (a.first != b.first)
                    return a.first < b.first;
                  else
                    return a.second < b.second;
                });
              acknowledge_position = progress_from(this->frete().files_info(), pmin);
              // need one call to read_acknowledge for save to have effect:async
              save_snapshot = true;
            }
          }
        };
        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          for (int i=0; i<num_threads; ++i)
            scope.run_background(elle::sprintf("cloud %s", i),
                                 std::bind(pipeline_cloud_upload, i));
          scope.wait();
        };
        // acknowledge last block and save snapshot
        frete.encrypted_read_acknowledge(0, 0, 0, this->frete().full_size());
        this->_save_frete_snapshot();
        this->state().meta().update_transaction(
          this->transaction_id(), TransactionStatus::cloud_buffered);
        this->gap_status(gap_transaction_cloud_buffered);
        exit_reason = infinit::metrics::TransferExitReasonFinished;
      } // try
      catch(reactor::Terminate const&)
      {
        exit_reason = infinit::metrics::TransferExitReasonTerminated;
        throw;
      }
      catch (boost::filesystem::filesystem_error const& e)
      {
        exit_message = e.what();
        exit_reason = infinit::metrics::TransferExitReasonError;
        if (e.code() == boost::system::errc::no_such_file_or_directory)
        {
          ELLE_WARN("%s: source file disappeared, cancel : %s",
                    *this, e.what());
          this->cancel("source file missing");
        }
        else
        {
          ELLE_WARN("%s: source file corrupted (%s), cancel",
                    *this, e.what());
          this->cancel(elle::sprintf("source file error: %s", e.what()));
        }
      }
      catch(...)
      {
        exit_reason = infinit::metrics::TransferExitReasonError;
        exit_message = elle::exception_string();
        ELLE_LOG("%s: cloud operation failed with %s", *this, exit_message);
      }
    }

    void
    PeerSendMachine::_cloud_synchronize()
    {
      // Nothing to do, don't keep uploading if the user is downloading
    }

    std::unique_ptr<frete::RPCFrete>
    PeerSendMachine::rpcs(infinit::protocol::ChanneledStream& channels)
    {
      return elle::make_unique<frete::RPCFrete>(this->frete(), channels);
    }

    bool
    PeerSendMachine::_fetch_peer_key(bool assert_success)
    {
      auto& frete = this->frete();
      if (frete.has_peer_key())
        return true;
      auto k =
        this->state().user(this->data()->recipient_id, true).public_key;
      if (k.empty() && !assert_success)
        return false;
      ELLE_ASSERT(!k.empty());
      ELLE_DEBUG("restoring key from %s", k);
      infinit::cryptography::PublicKey peer_K;
      peer_K.Restore(k);
      frete.set_peer_key(peer_K);
      return true;
    }

    frete::Frete&
    PeerSendMachine::frete()
    {
      if (this->_frete == nullptr)
      {
        ELLE_TRACE_SCOPE("%s: initialize frete", *this);
        ELLE_ASSERT(!this->data()->is_ghost);
        this->_frete = elle::make_unique<frete::Frete>(
          this->transaction_id(),
          this->state().identity().pair(),
          this->transaction().snapshots_directory() / "frete.snapshot",
          this->transaction().snapshots_directory() / "mirror_files",
          this->files_mirrored());
         _fetch_peer_key(false);
        if (this->_frete->count())
        {
          // Reloaded from snapshot. Not much to validate here, use previously
          // snapshoted directory content We could recreate a 2nd frete,
          // reperforming the add task, and compare the resulting effective file
          // lists.
        }
        else
        { // No snapshot yet, fill file list
          ELLE_DEBUG("%s: No snapshot loaded, populating files", *this);
          for (std::string const& file: this->files())
            this->_frete->add(file);
        }
      }
      return *this->_frete;
    }

    void
    PeerSendMachine::_save_frete_snapshot()
    {
      this->frete().save_snapshot();
    }

    float
    PeerSendMachine::progress() const
    {
      // If we're doing a plain upload, we need to use super's progress method.
      if (data()->is_ghost)
        return SendMachine::progress();
      if (this->_frete != nullptr)
        return this->_frete->progress();
      return 0.0f;
    }

    void
    PeerSendMachine::notify_user_connection_status(
      std::string const& user_id,
      bool user_status,
      elle::UUID const& device_id,
      bool device_status)
    {
      auto const& txn = this->data();
      // User hasn't accepted yet.
      if (user_id == txn->recipient_id && txn->recipient_device_id.is_nil())
      {
        this->_peer_connection_changed(user_status);
      }
      // User has accepted.
      else if (user_id == txn->recipient_id &&
               device_id == txn->recipient_device_id)
      {
        this->_peer_connection_changed(device_status);
      }
    }
  }
}
