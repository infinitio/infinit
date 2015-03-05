#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <elle/AtomicFile.hh>
#include <elle/finally.hh>
#include <elle/os/environ.hh>
#include <elle/serialization/json/SerializerIn.hh>
#ifdef INFINIT_WINDOWS
# include <elle/windows/string_conversion.hh>
#endif
#include <elle/serialization/json/SerializerOut.hh>
#include <elle/serialization/json.hh>
#include <elle/system/system.hh>

#include <reactor/exception.hh>
#include <reactor/thread.hh>
#include <reactor/Channel.hh>

#include <common/common.hh>

#include <frete/Frete.hh>
#include <frete/RPCFrete.hh>
#include <frete/TransferSnapshot.hh>

#include <papier/Identity.hh>

#include <station/Station.hh>

#include <aws/Credentials.hh>

#include <surface/gap/FilesystemTransferBufferer.hh>
#include <surface/gap/S3TransferBufferer.hh>
#include <surface/gap/PeerReceiveMachine.hh>
#include <surface/gap/State.hh>

#include <version.hh>


ELLE_LOG_COMPONENT("surface.gap.PeerReceiveMachine");

namespace surface
{
  namespace gap
  {
    struct PeerReceiveMachine::TransferData
    {
      TransferData(
        frete::TransferSnapshot::File&,
        boost::filesystem::path full_path,
        FileSize current_position = 0);

      boost::filesystem::path full_path;
      FileSize start_position; // next expected recieve buffer pos
      std::ofstream output;
    };

    static
    int
    rpc_pipeline_size()
    {
      std::string nr = elle::os::getenv("INFINIT_NUM_READER_THREAD", "");
      if (!nr.empty())
        return boost::lexical_cast<int>(nr);
      else
        return 8;
    }

    static
    std::streamsize
    rpc_chunk_size()
    {
      std::streamsize res = 1 << 18;
      std::string s = elle::os::getenv("INFINIT_CHUNK_SIZE", "");
      if (!s.empty())
      {
        res = boost::lexical_cast<std::streamsize>(s);
      }
      return res;
    }

    using TransactionStatus = infinit::oracles::Transaction::Status;
    PeerReceiveMachine::PeerReceiveMachine(
      Transaction& transaction,
      uint32_t id,
      std::shared_ptr<Data> data)
      : TransactionMachine(transaction, id, data)
      , ReceiveMachine(transaction, id, data)
      , PeerMachine(transaction, id, std::move(data))
      , _frete_snapshot_path(this->transaction().snapshots_directory()
                             / "frete.snapshot")
      , _snapshot(nullptr)
      , _completed(false)
      , _nothing_in_the_cloud(false)
    {
      try
      {
        if (exists(this->_frete_snapshot_path))
        {
          elle::AtomicFile file(this->_frete_snapshot_path);
          file.read() << [&] (elle::AtomicFile::Read& read)
          {
            elle::serialization::json::SerializerIn input(read.stream(), false);
            this->_snapshot.reset(new frete::TransferSnapshot(input));
          };
          if (this->_snapshot->file_count())
            ELLE_DEBUG("Reloaded snapshot, first file at %s",
                       this->_snapshot->file(0).progress());
        }
      }
      catch (boost::filesystem::filesystem_error const&)
      {
        ELLE_TRACE("%s: unable to read snapshot file: %s",
                   *this, elle::exception_string());
      }
      catch (std::exception const&) //XXX: Choose the right exception here.
      {
        ELLE_ERR("%s: snap shot was invalid: %s", *this, elle::exception_string());
      }
      this->_run_from_snapshot();
    }

    void
    PeerReceiveMachine::_run_from_snapshot()
    {
      bool started = false;
      boost::filesystem::path path = Snapshot::path(*this);
      if (exists(path))
      {
        ELLE_TRACE_SCOPE("%s: restore from snapshot: %s", *this, path);
        elle::AtomicFile source(path);
        source.read() << [&] (elle::AtomicFile::Read& read)
        {
          elle::serialization::json::SerializerIn input(read.stream(), false);
          Snapshot snapshot(input);
          started = true;
          ELLE_TRACE("%s: restore to state %s",
                     *this, snapshot.current_state())
          if (snapshot.current_state() == "accept")
            this->_run(this->_accept_state);
          else if (snapshot.current_state() == "cancel")
            this->_run(this->_cancel_state);
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
          else if (snapshot.current_state() == "wait for decision")
            this->_run(this->_wait_for_decision_state);
          else if (snapshot.current_state() == "another device")
            this->_run(this->_another_device_state);
          else
          {
            ELLE_WARN("%s: unkown state in snapshot: %s",
                      *this, snapshot.current_state());
            started = false;
          }
        };
      }
      else
      {
        if (this->data()->status != TransactionStatus::created &&
            this->data()->status != TransactionStatus::initialized)
          ELLE_WARN("%s: missing snapshot: %s", *this, path);
      }
      // Try to guess a decent starting state from the transaction status.
      if (started)
        return;
      ELLE_TRACE_SCOPE(
        "%s: deduce starting state from the transaction status: %s",
        *this, this->data()->status);
      switch (this->data()->status)
      {
        case TransactionStatus::accepted:
          if (this->concerns_this_device())
            this->_run(this->_transfer_state);
          else
            this->_run(this->_another_device_state);
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
          break;
        case TransactionStatus::started:
        case TransactionStatus::none:
        case TransactionStatus::deleted:
        case TransactionStatus::ghost_uploaded:
          elle::unreachable();
        // By default, run the first state. This way, if we add new statuses to
        // meta (like cloud_buffered) the machine will still be started.
        default:
          this->_run(this->_wait_for_decision_state);
          break;
      }
    }

    PeerReceiveMachine::~PeerReceiveMachine()
    {
      this->_stop();
    }

    void
    PeerReceiveMachine::accept()
    {
      if (!this->_accepted.opened())
      {
        bool onboarding = false;
        if (this->state().metrics_reporter())
          this->state().metrics_reporter()->transaction_accepted(
            this->transaction_id(),
            onboarding);
      }
      ReceiveMachine::accept();
    }

    void
    PeerReceiveMachine::_accept()
    {
      ReceiveMachine::_accept();
      try
      {
        auto res = this->state().meta().update_transaction(
          this->transaction_id(),
          TransactionStatus::accepted,
          this->state().device().id,
          this->state().device().name);
        if (!res.aws_credentials())
          this->_nothing_in_the_cloud = true;
      }
      catch (infinit::oracles::meta::Exception const& e)
      {
        if (e.err == infinit::oracles::meta::Error::transaction_already_has_this_status)
          ELLE_TRACE("%s: transaction already accepted: %s", *this, e.what());
        else if (e.err == infinit::oracles::meta::Error::transaction_operation_not_permitted)
          ELLE_TRACE("%s: transaction can't be accepted: %s", *this, e.what());
        else
          throw;
      }
    }

    void
    PeerReceiveMachine::reject()
    {
      if (!this->rejected().opened())
      {
        bool onboarding = false;
        if (this->state().metrics_reporter())
          this->state().metrics_reporter()->transaction_ended(
            this->transaction_id(),
            infinit::oracles::Transaction::Status::rejected,
            "",
            onboarding);
      }
      ReceiveMachine::reject();
    }

    void
    PeerReceiveMachine::_transfer_operation(frete::RPCFrete& frete)
    {
      ELLE_TRACE_SCOPE("%s: transfer operation", *this);
      elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        scope.run_background(
          elle::sprintf("run rpcs %s", this->id()),
          [&frete] ()
          {
            frete.run();
          });
        this->get(frete);
      };
    }

    void
    PeerReceiveMachine::_cloud_operation()
    {
      if (!elle::os::getenv("INFINIT_NO_CLOUD_BUFFERING", "").empty())
      {
        ELLE_DEBUG("%s: cloud buffering disabled by configuration", *this);
        this->gap_status(gap_transaction_waiting_data);
        return;
      }
      if (this->_nothing_in_the_cloud)
      {
        ELLE_TRACE("nothing was in the cloud");
        this->_nothing_in_the_cloud = false;
        return;
      }
      this->gap_status(gap_transaction_transferring);
      auto start_time = boost::posix_time::microsec_clock::universal_time();
      metrics::TransferExitReason exit_reason =
        metrics::TransferExitReasonUnknown;
      std::string exit_message;
      uint64_t total_bytes_transfered = 0;
      FileSize initial_progress = 0;
      if (this->_snapshot != nullptr)
        initial_progress = this->_snapshot->progress();
      elle::SafeFinally write_end_message([&,this]
      {
        if (auto& mr = state().metrics_reporter())
        {
          auto now = boost::posix_time::microsec_clock::universal_time();
          float duration =
            float((now - start_time).total_milliseconds()) / 1000.0f;
          mr->transaction_transfer_end(this->transaction_id(),
                                       metrics::TransferMethodCloud,
                                       duration,
                                       total_bytes_transfered,
                                       exit_reason,
                                       exit_message);
        }
      });
      try
      {
        ELLE_DEBUG("%s: create cloud bufferer", *this);
        bool cloud_debug =
          !elle::os::getenv("INFINIT_CLOUD_FILEBUFFERER", "").empty();
        if (cloud_debug)
        {
          _bufferer.reset(
            new FilesystemTransferBufferer(*this->data(),
                                           "/tmp/infinit-buffering"));
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
          this->_bufferer.reset(
            new S3TransferBufferer(
              elle::make_unique<S3>(this->state(), get_credentials),
              *this->data(),
              std::bind(&PeerReceiveMachine::_report_s3_error,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2)
              ));
        }
        if (auto& mr = state().metrics_reporter())
        {
          auto now = boost::posix_time::microsec_clock::universal_time();
          mr->transaction_transfer_begin(
            this->transaction_id(),
            infinit::metrics::TransferMethodCloud,
            float((now - start_time).total_milliseconds()) / 1000.0f);
        }
        ELLE_DEBUG("%s: download from the cloud", *this)
          this->get(*_bufferer);
        // We finished
        ELLE_ASSERT_EQ(progress(), 1);
        exit_reason = metrics::TransferExitReasonFinished;
        ELLE_ASSERT_NEQ(this->_snapshot, nullptr);
        total_bytes_transfered = this->_snapshot->progress() - initial_progress;
      }
      catch (boost::filesystem::filesystem_error const& e)
      {
        ELLE_WARN("%s: local file error: %s", *this, e.what());
        // Local file error, cancel transaction
        exit_reason = metrics::TransferExitReasonError;
        exit_message = e.what();
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        this->cancel(exit_message);
      }
      catch (TransferBufferer::DataExhausted const&)
      {
        exit_reason = metrics::TransferExitReasonExhausted;
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        ELLE_TRACE("%s: Data exhausted on cloud bufferer", *this);
        this->gap_status(gap_transaction_waiting_data);
      }
      catch (reactor::Terminate const&)
      { // aye aye
        exit_reason = metrics::TransferExitReasonTerminated;
         if (this->_snapshot)
           total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        throw;
      }
      catch (std::exception const& e)
      {
        // We don't ever retry cloud DL, but the idea is that for
        // cloud to work again, the peer must do something, which will
        // send us notifications and wake us up
        ELLE_WARN("%s: cloud download exception, exiting cloud state: %s",
                  *this, e.what());
        this->gap_status(gap_transaction_waiting_data);
        exit_reason = metrics::TransferExitReasonError;
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        exit_message = e.what();
        // treat this as nonfatal for transaction: no throw
      }
    }

    std::unique_ptr<frete::RPCFrete>
    PeerReceiveMachine::rpcs(infinit::protocol::ChanneledStream& channels)
    {
      return elle::make_unique<frete::RPCFrete>(channels);
    }

    float
    PeerReceiveMachine::progress() const
    {
      if (auto& snapshot = this->_snapshot)
        return float(snapshot->progress()) / snapshot->total_size();
      return 0.0f;
    }

    template<typename Source>
    elle::Version const&
    PeerReceiveMachine::peer_version(Source& source)
    {
      if (!this->_peer_version)
        this->_peer_version = source.version();
      return this->_peer_version.get();
    }

    template<typename Source>
    frete::Frete::TransferInfo const&
    PeerReceiveMachine::transfer_info(Source& source)
    {
      if (!this->_transfer_info)
      {
        if (this->peer_version(source) < elle::Version(0, 9, 25))
        {
          auto count = source.count();
          FilesInfo files_info;
          if (this->peer_version(source) >= elle::Version(0, 8, 9))
            files_info = source.files_info();
          else
          {
            for (unsigned i = 0; i < count; ++i)
            {
              auto path = source.path(i);
              auto size = source.file_size(i);
              files_info.push_back(std::make_pair(path, size));
            }
          }
          this->_transfer_info = frete::Frete::TransferInfo{
            count, source.full_size(), files_info};
        }
        else
        {
          this->_transfer_info = source.transfer_info();
        }
      }
      return this->_transfer_info.get();
    }

    void
    PeerReceiveMachine::get(frete::RPCFrete& frete,
                            std::string const& name_policy)
    {
      auto start_time = boost::posix_time::microsec_clock::universal_time();
      metrics::TransferExitReason exit_reason = metrics::TransferExitReasonUnknown;
      std::string exit_message;
      FileSize total_bytes_transfered = 0;
      FileSize initial_progress = 0;
      if (this->_snapshot != nullptr)
        initial_progress = this->_snapshot->progress();
      int attempt = _transfer_machine->attempt();
      bool skip_report = (attempt > 10 && attempt % (unsigned)pow(10, (unsigned)log10(attempt)));
      auto& mr = state().metrics_reporter();
      elle::SafeFinally write_end_message([&,this]
      {
        auto& mr = state().metrics_reporter();
        if (mr && !skip_report)
        {
          auto now = boost::posix_time::microsec_clock::universal_time();
          float duration =
            float((now - start_time).total_milliseconds()) / 1000.0f;
          mr->transaction_transfer_end(this->transaction_id(),
                                       metrics::TransferMethodP2P,
                                       duration,
                                       total_bytes_transfered,
                                       exit_reason,
                                       exit_message,
                                       attempt);
        }
      });
      if (mr && !skip_report)
      {
        auto now = boost::posix_time::microsec_clock::universal_time();
        mr->transaction_transfer_begin(
          this->transaction_id(),
          infinit::metrics::TransferMethodP2P,
          float((now - start_time).total_milliseconds()) / 1000.0f,
          attempt);
      }
      try
      {
        auto peer_version = frete.version();
        bool strong_encryption = true;
        if (peer_version < elle::Version(0, 8, 3))
        {
          // XXX: Create better exception.
          if (strong_encryption)
            ELLE_WARN("peer version doesn't support strong encryption");
          strong_encryption = false;
        }
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        else // normal termination: snapshot was removed
          total_bytes_transfered = this->transfer_info(frete).full_size();

        exit_reason = metrics::TransferExitReasonFinished;
        return this->get<frete::RPCFrete>(
          frete,
          strong_encryption ? EncryptionLevel_Strong : EncryptionLevel_Weak,
          name_policy,
          peer_version);
      }
      catch (boost::filesystem::filesystem_error const& e)
      {
        ELLE_WARN("%s: Local file error: %s", *this, e.what());
        // Local file error, cancel transaction
        exit_reason = metrics::TransferExitReasonError;
        exit_message = e.what();
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        this->cancel(exit_message);
      }
      catch(reactor::Terminate const&)
      {
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        exit_reason = metrics::TransferExitReasonTerminated;
        throw;
      }
      catch(...)
      {
        if (this->_snapshot)
          total_bytes_transfered = this->_snapshot->progress() - initial_progress;
        exit_reason = metrics::TransferExitReasonError;
        exit_message = elle::exception_string();
        throw;
      }
    }

    void
    PeerReceiveMachine::get(TransferBufferer& frete,
                            std::string const& name_policy)
    {
      return this->get<TransferBufferer>(
        frete, EncryptionLevel_Strong, name_policy,
        elle::Version(INFINIT_VERSION_MAJOR,
                      INFINIT_VERSION_MINOR,
                      INFINIT_VERSION_SUBMINOR));
    }

    template <typename Source>
    void
    PeerReceiveMachine::get(Source& source,
                            EncryptionLevel encryption,
                            std::string const& name_policy,
                            elle::Version const& peer_version)
    {
      auto clean_snpashot = [&] {
        try
        {
          boost::filesystem::remove(this->_frete_snapshot_path);
        }
        catch (std::exception const&)
        {
          ELLE_ERR("couldn't delete snapshot at %s: %s",
            this->_frete_snapshot_path, elle::exception_string());
        }
      };

      // Clear hypotetical blocks we fetched but did not process.
      this->_buffers.clear();
      boost::filesystem::path output_path(this->state().output_dir());
      auto count = this->transfer_info(source).count();

      // total_size can be 0 if all files are empty.
      auto total_size = this->transfer_info(source).full_size();
      if (this->_snapshot != nullptr)
      {
        if ((this->_snapshot->total_size() != total_size) ||
            (this->_snapshot->count() != count))
        {
          ELLE_ERR("snapshot data (%s) are invalid: size %s vs %s  count %s vs %s",
                   *this->_snapshot,
                   this->_snapshot->total_size(), total_size,
                   this->_snapshot->count(), count
                   );
          throw elle::Exception("invalid transfer data");
        }
      }
      else
      {
        this->_snapshot.reset(new frete::TransferSnapshot(count, total_size));
      }

      static const std::streamsize chunk_size = rpc_chunk_size();

      ELLE_DEBUG("transfer snapshot: %s", *this->_snapshot);

      FileID last_index = this->_snapshot->file_count();
      if (last_index > 0)
        --last_index;

      FilesInfo files_info = source.files_info();
      ELLE_ASSERT_GTE(files_info.size(), this->_snapshot->count());
      // reconstruct directory name mapping data so that files in transfer
      // but not yet in snapshot will reuse it
      for (unsigned i = 0; i < this->_snapshot->file_count(); ++i)
      {
        // get asked/got relative path from output_path
        boost::filesystem::path got = this->_snapshot->file(i).path();
        boost::filesystem::path asked = files_info.at(i).first;
        boost::filesystem::path got0 = *got.begin();
        boost::filesystem::path asked0 = *asked.begin();
        // add to the mapping even if its the same
        ELLE_DEBUG("Adding entry to path map: %s -> %s", asked0, got0);
        _root_component_mapping[asked0] = got0;
      }

      // FIXME: gcc 4.7 don't recognize the move assignment, hence the
      // unique_ptr instead of key = SecretKey(...)
      std::unique_ptr<infinit::cryptography::SecretKey> key;
      switch (encryption)
      {
      case EncryptionLevel_Weak:
        key.reset(new infinit::cryptography::SecretKey(
                    infinit::cryptography::cipher::Algorithm::aes256,
                    this->transaction_id()));
        break;
      case EncryptionLevel_Strong:
        key.reset(new infinit::cryptography::SecretKey(
                    this->state().identity().pair().k().decrypt<
                    infinit::cryptography::SecretKey>(source.key_code())));
          break;
      case EncryptionLevel_None:
          break;
      }

      _fetch_current_file_index = last_index;
      // Snapshot only has info on files for which transfer started,
      // and we transfer in order, so we know all files in snapshot except
      bool things_to_do = _fetch_next_file(name_policy, files_info);
      if (!things_to_do)
        ELLE_TRACE("Nothing to do");
      if (things_to_do)
      {
        // Initialize expectations of reader thread with first block
        _store_expected_file = _fetch_current_file_index;
        _store_expected_position = _fetch_current_position;
        // Start processing threads
        bool exception = false;
        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          // Have multiple reader threads, sharing read position
          // so we push stuff in order to 'buffers' (we are assuming a
          // synchronous singlethreaded RPC handler at the other end)
          // The idea is to absorb 'gaps' in link availability.
          // Counting 256k packet size and 10Mo/s, two pending requests
          // 'buffers' for 1/20th of a second
          static int num_reader = rpc_pipeline_size();
          bool explicit_ack = peer_version >= elle::Version(0, 8, 9);
          // Prevent unlimited ram buffering if a block fetcher gets stuck
          this->_buffers.max_size(num_reader * 3);
          for (int i = 0; i < num_reader; ++i)
              scope.run_background(
                elle::sprintf("transfer reader %s", i),
                std::bind(&PeerReceiveMachine::_fetcher_thread<Source>,
                          this, std::ref(source), i, name_policy, explicit_ack,
                          encryption, chunk_size, std::ref(*key),
                          files_info));
          scope.run_background(
            "receive writer",
            std::bind(&PeerReceiveMachine::_disk_thread<Source>,
                      this, std::ref(source),
                      peer_version, chunk_size));
          try
          {
            reactor::wait(scope);
          }
          catch (boost::filesystem::filesystem_error const& e)
          {
            ELLE_ERR("%s: Filesystem error: %s", *this, e.what())
            this->cancel(elle::sprintf("Filesystem error: %s", e.what()));
            exception = true;
          }
          clean_snpashot();
          ELLE_TRACE("finish_transfer exited cleanly");
        }; // scope
        if (exception)
          return;
      }// if current_transfer
      ELLE_LOG("%s: transfer finished", *this);
      if (peer_version >= elle::Version(0, 8, 7))
      {
        source.finish();
        this->_completed = true;
      }
      clean_snpashot();
    }

    bool
    PeerReceiveMachine::completed() const
    {
      return this->_completed;
    }

    PeerReceiveMachine::FileSize
    PeerReceiveMachine::_initialize_one(FileID index,
                                        const std::string& file_path,
                                        FileSize file_size,
                                        const std::string& name_policy)
    {
      boost::filesystem::path output_path(this->state().output_dir());
      boost::filesystem::path fullpath;

      if (this->_snapshot->has(index))
      {
        auto const& file = this->_snapshot->file(index);
        fullpath = file.full_path();

        if (file_size != file.size())
        {
          ELLE_ERR("%s: transfer data (%s) at index %s are invalid.",
                   *this, file, index);
          throw elle::Exception("invalid transfer data");
        }
      }
      else
      {
        auto relative_path = boost::filesystem::path(file_path);
        fullpath = ReceiveMachine::eligible_name(
          output_path, relative_path, name_policy,
          this->_root_component_mapping);
        relative_path = ReceiveMachine::trim(fullpath, output_path);
        this->_snapshot->add(index, output_path, relative_path, file_size);
      }

      auto& tr = this->_snapshot->file(index);

      ELLE_DEBUG("%s: index (%s) - path %s - size %s",
        *this, index, fullpath, file_size);


      try
      {
        boost::filesystem::create_directories(fullpath.parent_path());
      }
      catch (...)
      {
        throw;
        ELLE_ERR("this one: %s", fullpath);
      }
      if (tr.complete())
      {
        ELLE_DEBUG("%s: transfer was marked as complete", *this);
        // Handle empty files here
        if (!boost::filesystem::exists(fullpath))
        {
          // Touch the file.
          elle::system::write_file(fullpath);
        }
        return FileSize(-1);
      }
      if (boost::filesystem::exists(fullpath))
      {
        // Check size against snapshot data
        auto size = boost::filesystem::file_size(fullpath);
        if (size < tr.progress())
        { // missing data on disk. Should not happen.
          std::string msg = elle::sprintf("%s: file %s too short, expected %s, got %s",
                    *this, fullpath, tr.progress(), size);
          ELLE_WARN(msg.c_str());
          throw elle::Exception(msg);
        }
        if (size > tr.progress())
        {
          // File too long, which means snapshot was not synced properly.
          // Be conservative and truncate the file to expected length, maybe
          // file write was only partial
          ELLE_WARN("%s: File %s bigger than snapshot size: expected %s, got %s",
                    *this, fullpath, tr.progress(), size);
          // We need to effectively truncate the file, we have
          // file_size checks all over the map
          elle::system::truncate(fullpath, tr.progress());
          size = boost::filesystem::file_size(fullpath);
          if (size != tr.progress())
            throw elle::Exception(
              elle::sprintf("Truncate failed on %s: expected %s, got %s",
                            fullpath, tr.progress(), size));
        }
      }
      // Touch the file.
      elle::system::write_file(fullpath);
      // Imediatly save snapshot to prevent a new call to eligible_name()
      this->_save_frete_snapshot();
      return tr.progress();
    }

    bool PeerReceiveMachine::IndexedBuffer::operator<(
      const PeerReceiveMachine::IndexedBuffer& b) const
    {
        // Ensure -1 which is our stop request stays last
        if (file_index != b.file_index)
          return (unsigned)file_index > (unsigned)b.file_index;
        else
          return start_position > b.start_position;
    };

    bool
    PeerReceiveMachine::_fetch_next_file(
      const std::string& name_policy,
      FilesInfo const& infos)
    {
      FileSize pos = 0;
      // switch to next file until we find one for which there is something to do
      while (_fetch_current_file_index < infos.size())
      {
        pos = this->_initialize_one(
          _fetch_current_file_index,
          infos.at(_fetch_current_file_index).first,
          infos.at(_fetch_current_file_index).second,
          name_policy);
        if (pos != FileSize(-1))
          break;
        ++_fetch_current_file_index;
      }
      if (_fetch_current_file_index >= _snapshot->count())
      {
        // we're done
        _fetch_current_file_index = -1;
        return false;
      }
      _fetch_current_position = pos; // start position for this file
      _fetch_current_file_full_size =
        _snapshot->file(_fetch_current_file_index).size();
      return true;
    }

    template<typename Source>
    void
    PeerReceiveMachine::_fetcher_thread(
      Source& source, int id,
      const std::string& name_policy,
      bool explicit_ack,
      EncryptionLevel encryption,
      size_t chunk_size,
      const infinit::cryptography::SecretKey& key,
      FilesInfo const& files_info)
    {
      while (true)
      {
        if (_fetch_current_file_index == -1)
        {
          ELLE_DEBUG("Thread %s has nothing to do, exiting", id);
          break; // some other thread figured out this was over
        }
        ELLE_DEBUG("Reading buffer at %s in mode %s", _fetch_current_position,
          explicit_ack? std::string("read_encrypt_ack") :
           boost::lexical_cast<std::string>(encryption)
          );
        if (_fetch_current_position >= _fetch_current_file_full_size)
        {
          ELLE_DEBUG("Thread %s would read past end", id);
          ++_fetch_current_file_index;
          if (!_fetch_next_file(name_policy, files_info))
          {
            // we're done
            _fetch_current_file_index = -1;
            break;
          }

        }
        // local cache for next block
        FileSize local_position = _fetch_current_position;
        FileID   local_index    = _fetch_current_file_index;
        _fetch_current_position += chunk_size;

        // This line blocks, no shared state access past that point!
        // For some reasons this can't be rewritten cleanly: the compiler
        // burst into flames about deleted =(const&), thus ignoring
        // =(&&)  when writing code = f();
        infinit::cryptography::Code code;
        elle::Buffer buffer;
        if (explicit_ack)
          code = source.encrypted_read_acknowledge(local_index,
                                              local_position, chunk_size,
                                              this->_snapshot->progress());
        else switch(encryption)
        {
        case EncryptionLevel_Strong:
          code = source.encrypted_read(local_index, local_position, chunk_size);
          break;
        case EncryptionLevel_Weak:
          code = source.read(local_index, local_position, chunk_size);
          break;
        case EncryptionLevel_None:
          buffer = std::move(source.read(local_index, local_position, chunk_size).buffer());
          break;
        }
        if (encryption != EncryptionLevel_None)
        {
          try
          {
            buffer = key.legacy_decrypt_buffer(code);
          }
          catch(infinit::cryptography::Exception const& e)
          {
            ELLE_WARN("%s: decryption error on block %s/%s: %s", *this, local_index, local_position, e.what());
            throw;
          }
          ELLE_ASSERT_NO_OTHER_EXCEPTION
        }
        ELLE_DEBUG("Queuing buffer %s/%s size:%s. Writer waits for %s/%s",
          local_index, local_position, buffer.size(),
          _store_expected_file, _store_expected_position);
        // Subtelty here: put will block us *after* the insert operation
        // if queue is full, so we must notify the reader thread before the
        // put
         if (local_index == _store_expected_file
          && local_position == _store_expected_position)
        {
          ELLE_DEBUG("Opening disk writer barrier at %s/%s", local_index, local_position);
          _disk_writer_barrier.open();
        }
        this->_buffers.put(
          IndexedBuffer{std::move(buffer),
                        local_position, local_index});
      }
      ELLE_DEBUG("reader %s exiting cleanly", id);
    }

    template<typename Source>
    void
    PeerReceiveMachine::_disk_thread(Source& source, elle::Version peer_version,
                                     size_t chunk_size)
    {
      // somebody initialized our _store_expected_ state
      ELLE_TRACE_SCOPE("%s: start writing blocks to disk", *this);
      // Cached current transfer info to avoid refetching each time.
      // it should be there, but a logic error that gives us a
      // nothing-to-do-on-this-first-file state is possible and nonfatal
      // so do not use at
      elle::system::FileHandle current_file_handle;
      FileSize current_file_full_size;
      boost::filesystem::path current_file_full_path;
      {
        frete::TransferSnapshot::File& f = _snapshot->file(_store_expected_file);
        current_file_full_size = f.size();
        current_file_full_path = f.full_path();
        current_file_handle = elle::system::FileHandle(current_file_full_path,
                                                       elle::system::FileHandle::APPEND);
      }
      while (true)
      {
        ELLE_DEBUG("%s waiting for block %s/%s", *this, _store_expected_file,
          _store_expected_position);
        reactor::wait(_disk_writer_barrier);
        while (true)
        { // we might have successive blocks ready in the pipe, and nobody
          // will notify us of the ones after the top() one.
          IndexedBuffer data = this->_buffers.get();
          if (data.file_index == FileID(-1))
          {
            ELLE_DEBUG("%s: done writing blocks to disk", *this);
            break;
          }
          const elle::Buffer& buffer = data.buffer;
          ELLE_DEBUG("%s: receiver got data for file %s at position %s with size %s, "
                     "will write to %s",
                     *this,
                     data.file_index, data.start_position,
                     data.buffer.size(),
                     current_file_full_path);
          // If this assert fails, packets were received out of order.
          ELLE_ASSERT_EQ(_store_expected_file, data.file_index);
          ELLE_ASSERT_EQ(_store_expected_position, data.start_position);

          // Write the file.
          ELLE_DUMP("content: %x (%sB)", buffer, buffer.size());
          current_file_handle.write(buffer);
          this->_snapshot->file_progress_increment(_store_expected_file, buffer.size());
          // OLD clients need this RPC to update progress
          if (peer_version < elle::Version(0, 8, 7))
            source.set_progress(this->_snapshot->progress());
          // Write snapshot state to file
          ELLE_DEBUG("%s: write down snapshot", *this)
          {
            ELLE_DUMP("%s: snapshot: %s", *this, *this->_snapshot);
            this->_save_frete_snapshot();
          }
           _store_expected_position += buffer.size();
           ELLE_ASSERT_EQ(_store_expected_position,
                          _snapshot->file(_store_expected_file).progress());
          if (buffer.size() < chunk_size
            || _store_expected_position >= current_file_full_size)
          {
            if (_store_expected_position != current_file_full_size)
            {
              ELLE_ERR("%s: end of transfer with unexpected size, "
                       "got %s, expected %s",
                       *this, _store_expected_position, current_file_full_size);
              throw boost::filesystem::filesystem_error(
                elle::sprintf("End with incorrect size: %s of %s",
                              current_file_full_size,
                              _store_expected_position
                              ),
                current_file_full_path,
                boost::system::errc::make_error_code(boost::system::errc::io_error));
            }
          }

          // Update our expected file if needed
          while (_store_expected_position == current_file_full_size)
          {
            ++_store_expected_file;
            if (_store_expected_file == _snapshot->count())
            {
              ELLE_TRACE("%s: writer thread is done", *this);
              return;
            }
            frete::TransferSnapshot::File& f = _snapshot->file(_store_expected_file);
            _store_expected_position = f.progress();
            current_file_full_size = f.size();
            current_file_full_path = f.full_path();
            current_file_handle = elle::system::FileHandle(current_file_full_path,
                                                           elle::system::FileHandle::APPEND);
            if (_store_expected_position != current_file_full_size)
            {
              // We need blocks for that one.
              break;
            }
          }
          // Check next available data
          if (this->_buffers.empty())
          {
            _disk_writer_barrier.close();
            break; // break to outer while that will wait on barrier
          }
          const IndexedBuffer& next = this->_buffers.peek();
          if (next.start_position != _store_expected_position
             || next.file_index != _store_expected_file)
          {
            _disk_writer_barrier.close();
            break; // break to outer while that will wait on barrier
          }
        } // inner while true
      } // outer while true
      // No need to Finally the block below, it stops an other thread in
      // the same scope
    }

    void
    PeerReceiveMachine::_save_frete_snapshot()
    {
      elle::AtomicFile file(this->_frete_snapshot_path.string());
      file.write() << [&] (elle::AtomicFile::Write& write)
      {
        elle::serialization::json::SerializerOut output(write.stream(), false);
        this->_snapshot->serialize(output);
      };
    }

    void
    PeerReceiveMachine::cleanup()
    { // our _get knows when it's finished, nothing to do
      ELLE_TRACE_SCOPE("%s: cleaning up", *this);
      // FIXME: cloud-check instead of relying if we have bufferer locally
      if (_bufferer)
        _bufferer->cleanup();
      // FIXME: cleanup raw cloud data
    }

    void
    PeerReceiveMachine::_cloud_synchronize()
    {
      this->_cloud_operation();
    }

    void
    PeerReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision", *this);
      this->gap_status(gap_transaction_waiting_accept);
      if (this->data()->sender_id == this->state().me().id &&
          !this->data()->recipient_device_id.is_nil())
      {
        if (this->data()->recipient_device_id == this->state().device_uuid())
        {
          ELLE_TRACE("%s: auto accept transaction specifically for this device",
                     *this);
          this->transaction().accept();
        }
        else
        {
          ELLE_TRACE("%s: transaction is specifically for another device",
                     *this);
          // FIXME: not really accepted elsewhere, just only acceptable
          // elsewhere. Change when acceptance gets reworked.
          this->_accepted_elsewhere.open();
        }
      }
    }

    void
    PeerReceiveMachine::notify_user_connection_status(
      std::string const& user_id,
      bool user_status,
      elle::UUID const& device_id,
      bool device_status)
    {
      if (user_id == this->data()->sender_id
          && device_id == this->data()->sender_device_id)
        this->_peer_connection_changed(device_status);
    }

    void
    PeerReceiveMachine::IndexedBuffer::operator=(IndexedBuffer&&b)
    {
      buffer = std::move(b.buffer);
      start_position = b.start_position;
      file_index = b.file_index;
    }

    PeerReceiveMachine::IndexedBuffer::IndexedBuffer(elle::Buffer && b,
                                                     FileSize pos, FileID index)
    : buffer(std::move(b))
    , start_position(pos)
    , file_index(index)
    {}

    PeerReceiveMachine::IndexedBuffer::IndexedBuffer(IndexedBuffer && b)
    : buffer(std::move(b.buffer))
    , start_position(b.start_position)
    , file_index(b.file_index)
    {}
  }
}
