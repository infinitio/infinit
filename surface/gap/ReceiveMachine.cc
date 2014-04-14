#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <elle/finally.hh>
#include <elle/os/environ.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/system/system.hh>

#include <reactor/exception.hh>
#include <reactor/thread.hh>
#include <reactor/Channel.hh>

#include <common/common.hh>

#include <frete/Frete.hh>
#include <frete/TransferSnapshot.hh>

#include <papier/Identity.hh>

#include <station/Station.hh>

#include <aws/Credentials.hh>

#include <surface/gap/FilesystemTransferBufferer.hh>
#include <surface/gap/S3TransferBufferer.hh>
#include <surface/gap/ReceiveMachine.hh>

#include <version.hh>


ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine");

namespace surface
{
  namespace gap
  {
    struct ReceiveMachine::TransferData
    {
      TransferData(
        frete::TransferSnapshot::File&,
        boost::filesystem::path full_path,
        FileSize current_position = 0);

      boost::filesystem::path full_path;
      FileSize start_position; // next expected recieve buffer pos
      boost::filesystem::ofstream output;
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
    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransactionMachine::Data> data,
                                   boost::filesystem::path const& snapshot_path,
                                   bool):
      TransactionMachine(state, id, std::move(data), snapshot_path),
      _wait_for_decision_state(
        this->_machine.state_make(
          "wait for decision", std::bind(&ReceiveMachine::_wait_for_decision, this))),
      _accept_state(
        this->_machine.state_make(
          "accept", std::bind(&ReceiveMachine::_accept, this))),
      _accepted("accepted"),
      _snapshot_path(boost::filesystem::path(
                       common::infinit::frete_snapshot_path(
                         this->data()->recipient_id,
                         this->data()->id))),
      _snapshot(nullptr)
    {
      // Normal way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_accept_state,
                                    reactor::Waitables{&this->_accepted});
      this->_machine.transition_add(this->_accept_state,
                                      this->_transfer_core_state);

      this->_machine.transition_add(this->_transfer_core_state,
                                    this->_finish_state);

      // Reject way.
      this->_machine.transition_add(this->_wait_for_decision_state,
                                    this->_reject_state,
                                    reactor::Waitables{&this->rejected()});

      // Cancel.
      this->_machine.transition_add(_wait_for_decision_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(_accept_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(_reject_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);
      this->_machine.transition_add(_transfer_core_state, _cancel_state, reactor::Waitables{&this->canceled()}, true);

      // Exception.
      this->_machine.transition_add_catch(_wait_for_decision_state, _fail_state);
      this->_machine.transition_add_catch(_accept_state, _fail_state);
      this->_machine.transition_add_catch(_reject_state, _fail_state);
      this->_machine.transition_add_catch(_transfer_core_state, _fail_state);

      this->_machine.state_changed().connect(
        [this] (reactor::fsm::State& state)
        {
          ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine.State");
          ELLE_TRACE("%s: entering %s", *this, state);
        });

      this->_machine.transition_triggered().connect(
        [this] (reactor::fsm::Transition& transition)
        {
          ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine.Transition");
          ELLE_TRACE("%s: %s triggered", *this, transition);
        });

      try
      {
        this->_snapshot.reset(
          new frete::TransferSnapshot(
            elle::serialize::from_file(this->_snapshot_path.string())));
          if (this->_snapshot->file_count())
            ELLE_DEBUG("Reloaded snapshot, first file at %s",
              this->_snapshot->file(0).progress());
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
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   TransactionMachine::State const current_state,
                                   std::shared_ptr<TransactionMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), "", true)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s, starting at %s",
                       *this, *this->data(), current_state);

      switch (current_state)
      {
        case TransactionMachine::State::NewTransaction:
          break;
        case TransactionMachine::State::SenderCreateTransaction:
        case TransactionMachine::State::SenderWaitForDecision:
          elle::unreachable();
        case TransactionMachine::State::RecipientWaitForDecision:
          this->_run(this->_wait_for_decision_state);
          break;
        case TransactionMachine::State::RecipientAccepted:
          this->_run(this->_accept_state);
          break;
        case TransactionMachine::State::PublishInterfaces:
        case TransactionMachine::State::Connect:
        case TransactionMachine::State::PeerDisconnected:
        case TransactionMachine::State::PeerConnectionLost:
        case TransactionMachine::State::Transfer:
        case TransactionMachine::State::DataExhausted:
        case TransactionMachine::State::CloudSynchronize:
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
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   std::shared_ptr<TransactionMachine::Data> data,
                                   boost::filesystem::path const& snapshot_path):
      ReceiveMachine(state, id, std::move(data), snapshot_path, true)
    {
      ELLE_TRACE_SCOPE("%s: constructing machine for transaction %s",
                       *this, data);

      switch (this->data()->status)
      {
        case TransactionStatus::initialized:
          this->_run(this->_wait_for_decision_state);
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

    ReceiveMachine::~ReceiveMachine()
    {
      this->_stop();
    }

    void
    ReceiveMachine::transaction_status_update(TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: update with new transaction status %s",
                       *this, status);

      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      switch (status)
      {
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
        case TransactionStatus::accepted:
        case TransactionStatus::rejected:
        case TransactionStatus::initialized:
          ELLE_DEBUG("%s: ignore status %s", *this, status);
          break;
        case TransactionStatus::created:
        case TransactionStatus::started:
        case TransactionStatus::none:
          elle::unreachable();
      }
    }

    void
    ReceiveMachine::accept()
    {
      ELLE_TRACE_SCOPE("%s: open accept barrier %s", *this, this->transaction_id());
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      if (!this->_accepted.opened())
      {
        if (this->state().metrics_reporter())
          this->state().metrics_reporter()->transaction_accepted(
            this->transaction_id()
            );
      }

      this->_accepted.open();
    }

    void
    ReceiveMachine::reject()
    {
      ELLE_TRACE_SCOPE("%s: open rejected barrier %s", *this, this->transaction_id());
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      if (!this->rejected().opened())
      {
        if (this->state().metrics_reporter())
          this->state().metrics_reporter()->transaction_ended(
            this->transaction_id(),
            infinit::oracles::Transaction::Status::rejected,
            ""
            );
      }

      this->rejected().open();
    }

    void
    ReceiveMachine::_wait_for_decision()
    {
      ELLE_TRACE_SCOPE("%s: waiting for decision %s", *this, this->transaction_id());
      this->current_state(TransactionMachine::State::RecipientWaitForDecision);
    }

    void
    ReceiveMachine::_accept()
    {
      ELLE_TRACE_SCOPE("%s: accepted %s", *this, this->transaction_id());
      this->current_state(TransactionMachine::State::RecipientAccepted);

      try
      {
        this->state().meta().update_transaction(this->transaction_id(),
                                                TransactionStatus::accepted,
                                                this->state().device().id,
                                                this->state().device().name);
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

    boost::filesystem::path
    ReceiveMachine::eligible_name(boost::filesystem::path start_point,
                                  boost::filesystem::path const path,
                                  std::string const& name_policy,
                                  std::map<boost::filesystem::path, boost::filesystem::path>& mapping)
    {
      boost::filesystem::path first = *path.begin();
      // Take care of toplevel files with no directory information, we can't
      // add that to the mapping.
      bool toplevel_file = (first == path);
      bool exists = boost::filesystem::exists(start_point / first);
      ELLE_DEBUG("Looking for a replacment name for %s, firstcomp=%s, exists=%s", path, first, exists);
      if (! exists)
      { // we will create the path along the way so we must add itself into mapping
        if (!toplevel_file)
          mapping[first] = first;
        return start_point / path;
      }
      auto it = mapping.find(first);
      ELLE_DEBUG("Looking for %s in mapping", first);
      if (it != mapping.end())
      {
        ELLE_DEBUG("Found it in mapping");
        boost::filesystem::path result = it->second;
        auto it = path.begin();
        ++it;
        for (; it != path.end(); ++it)
          result /= *it;
        ELLE_DEBUG("Returning final path: %s", result);
        return start_point / result;
      }

      // Remove the extensions, add the name_policy and set the extension.
      std::string extensions;
      boost::filesystem::path pattern = first;
      for (; !pattern.extension().empty(); pattern = pattern.stem())
        extensions = pattern.extension().string() + extensions;
      pattern += name_policy;
      pattern += extensions;

      // Ugly.
      for (size_t i = 2; i < std::numeric_limits<size_t>::max(); ++i)
      {
        boost::filesystem::path replace = elle::sprintf(pattern.string().c_str(), i);
        if (!boost::filesystem::exists(start_point / replace))
        {
          if (!toplevel_file)
            mapping[first] = replace;
          ELLE_DEBUG("Adding in mapping: %s -> %s", first, replace);
          boost::filesystem::path result = replace;
          auto it = path.begin();
          ++it;
          for (; it != path.end(); ++it)
            result /= *it;
          ELLE_DEBUG("Found %s", result);
          return start_point / result;
        }
      }


      throw elle::Exception(
        elle::sprintf("unable to find a suitable name that matches %s", first));
    }

    boost::filesystem::path
    ReceiveMachine::trim(boost::filesystem::path const& item,
                         boost::filesystem::path const& root)
    {
      if (item == root)
        return "";

      auto it = item.begin();
      boost::filesystem::path rel;
      for(; rel != root && it != item.end(); ++it)
        rel /= *it;
      if (it == item.end())
        throw elle::Exception(
          elle::sprintf("%s is not the root of %s", root, item));

      boost::filesystem::path trimed;
      for (; it != item.end(); ++it)
        trimed /= *it;

      return trimed;
    }

    void
    ReceiveMachine::_transfer_operation(frete::RPCFrete& frete)
    {
      ELLE_TRACE_SCOPE("%s: transfer operation", *this);
      elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        scope.run_background(
          elle::sprintf("download %s", this->id()),
          [&frete, this] ()
          {
            this->get(frete);
          });
        scope.run_background(
          elle::sprintf("run rpcs %s", this->id()),
          [&frete] ()
          {
            frete.run();
          });
        scope.wait();
      };
    }

    void
    ReceiveMachine::_cloud_operation()
    {
      if (!elle::os::getenv("INFINIT_NO_CLOUD_BUFFERING", "").empty())
      {
        ELLE_DEBUG("%s: cloud buffering disabled by configuration", *this);
        return;
      }
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
          auto get_credentials = [this]()
          {
            auto& meta = this->state().meta();
            auto token = meta.get_cloud_buffer_token(this->transaction_id());
            return aws::Credentials(token.access_key_id,
                                    token.secret_access_key,
                                    token.session_token,
                                    token.expiration);
          };
         _bufferer.reset(new S3TransferBufferer(*this->data(),
                                               get_credentials));
        }
        ELLE_DEBUG("%s: download from the cloud", *this)
          this->get(*_bufferer);
        // We finished
        ELLE_ASSERT_EQ(progress(), 1);
        this->finished().open();
      }
      catch (TransferBufferer::DataExhausted const&)
      {
        ELLE_TRACE("%s: Data exhausted on cloud bufferer", *this);
        this->current_state(TransactionMachine::State::DataExhausted);
      }
      catch (reactor::Terminate const&)
      { // aye aye
        throw;
      }
      catch (std::exception const& e)
      {
        // We don't ever retry could DL, but the idea is that for
        // cloud to work again, the peer must do something, which will
        // send us notifications and wake us up
        ELLE_WARN("%s: cloud download exception, exiting cloud state: %s",
                  *this, e.what());
        this->current_state(TransactionMachine::State::DataExhausted);
      }
    }

    std::unique_ptr<frete::RPCFrete>
    ReceiveMachine::rpcs(infinit::protocol::ChanneledStream& channels)
    {
      return elle::make_unique<frete::RPCFrete>(channels);
    }

    float
    ReceiveMachine::progress() const
    {
      if (auto& snapshot = this->_snapshot)
        return float(snapshot->progress()) / snapshot->total_size();
      return 0.0f;
    }

    void
    ReceiveMachine::get(frete::RPCFrete& frete,
                        std::string const& name_policy)
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
      return this->_get<frete::RPCFrete>(
        frete, strong_encryption, name_policy, peer_version);
    }

    void
    ReceiveMachine::get(TransferBufferer& frete,
                        std::string const& name_policy)
    {
      return this->_get<TransferBufferer>(
        frete, true, name_policy,
        elle::Version(INFINIT_VERSION_MAJOR,
                      INFINIT_VERSION_MINOR,
                      INFINIT_VERSION_SUBMINOR));
    }

    template <typename Source>
    void
    ReceiveMachine::_get(Source& source,
                         bool strong_encryption,
                         std::string const& name_policy,
                         elle::Version const& peer_version
                         )
    {
      // Clear hypotetical blocks we fetched but did not process.
      this->_buffers.clear();
      boost::filesystem::path output_path(this->state().output_dir());
      auto count = source.count();

      // total_size can be 0 if all files are empty.
      auto total_size = source.full_size();

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

      std::vector<std::pair<std::string, FileSize>> infos;
      if (peer_version >= elle::Version(0, 8, 9))
        infos = source.files_info();
      else
      {
        for (int i = 0; i < count; ++i)
        {
          auto path = source.path(i);
          auto size = source.file_size(i);
          infos.push_back(std::make_pair(path, size));
        }
      }

      ELLE_ASSERT(infos.size() >= this->_snapshot->count());
      // reconstruct directory name mapping data so that files in transfer
      // but not yet in snapshot will reuse it
      for (int i = 0; i < this->_snapshot->file_count(); ++i)
      {
        // get asked/got relative path from output_path
        boost::filesystem::path got = this->_snapshot->file(i).path();
        boost::filesystem::path asked = infos.at(i).first;
        boost::filesystem::path got0 = *got.begin();
        boost::filesystem::path asked0 = *asked.begin();
        // add to the mapping even if its the same
        ELLE_DEBUG("Adding entry to path map: %s -> %s", asked0, got0);
        _root_component_mapping[asked0] = got0;
      }

      auto key = strong_encryption ?
        infinit::cryptography::SecretKey(
          this->state().identity().pair().k().decrypt<
          infinit::cryptography::SecretKey>(source.key_code())) :
        infinit::cryptography::SecretKey(
          infinit::cryptography::cipher::Algorithm::aes256,
          this->transaction_id());

      _fetch_current_file_index = last_index;
      // Snapshot only has info on files for which transfer started,
      // and we transfer in order, so we know all files in snapshot except
      bool things_to_do = _fetch_next_file(name_policy,
                                           source.files_info());
      if (!things_to_do)
        ELLE_TRACE("Nothing to do");
      if (things_to_do)
      {
        // Initialize expectations of reader thread with first block
        _store_expected_file = _fetch_current_file_index;
        _store_expected_position = _fetch_current_position;
        // Start processing threads
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
          auto files_info = source.files_info();
          // Prevent unlimited ram buffering if a block fetcher gets stuck
          this->_buffers.max_size(num_reader * 3);
          for (int i = 0; i < num_reader; ++i)
              scope.run_background(
                elle::sprintf("transfer reader %s", i),
                std::bind(&ReceiveMachine::_fetcher_thread<Source>,
                          this, std::ref(source), i, name_policy, explicit_ack,
                          strong_encryption, chunk_size, std::ref(key),
                            files_info));
          scope.run_background(
            "receive writer",
            std::bind(&ReceiveMachine::_disk_thread<Source>,
                      this, std::ref(source),
                      peer_version, chunk_size));
          reactor::wait(scope);
          ELLE_TRACE("finish_transfer exited cleanly");
        }; // scope
      }// if current_transfer
      // this->finished.open();
      ELLE_LOG("%s: transfer finished", *this);
      if (peer_version >= elle::Version(0, 8, 7))
      {
        source.finish();
      }
      this->finished().open();

      try
      {
        boost::filesystem::remove(this->_snapshot_path);
      }
      catch (std::exception const&)
      {
        ELLE_ERR("couldn't delete snapshot at %s: %s",
                 this->_snapshot_path, elle::exception_string());
      }
    }

    ReceiveMachine::FileSize
    ReceiveMachine::_initialize_one(FileID index,
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


      if (tr.complete())
      {
        ELLE_DEBUG("%s: transfer was marked as complete", *this);
        // Handle empty files here
        if (!boost::filesystem::exists(fullpath))
        {
          boost::filesystem::create_directories(fullpath.parent_path());
          boost::filesystem::ofstream output(fullpath, std::ios::app | std::ios::binary);
        }
        return FileSize(-1);
      }
      boost::filesystem::create_directories(fullpath.parent_path());
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
      _transfer_stream_map[index] =
        elle::make_unique<boost::filesystem::ofstream>(fullpath, std::ios::app | std::ios::binary);
      return tr.progress();
    }

    bool ReceiveMachine::IndexedBuffer::operator<(
      const ReceiveMachine::IndexedBuffer& b) const
    {
        // Ensure -1 which is our stop request stays last
        if (file_index != b.file_index)
          return (unsigned)file_index > (unsigned)b.file_index;
        else
          return start_position > b.start_position;
    };

    bool
    ReceiveMachine::_fetch_next_file(
      const std::string& name_policy,
      const std::vector<std::pair<std::string, FileSize>>& infos)
    {
      boost::filesystem::path output_path(this->state().output_dir());
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
    ReceiveMachine::_fetcher_thread(Source& source, int id,
                                    const std::string& name_policy,
                                    bool explicit_ack,
                                    bool strong_encryption,
                                    size_t chunk_size,
                                    const infinit::cryptography::SecretKey& key,
                                    std::vector<std::pair<std::string, FileSize>> const& files_info)
    {
      while (true)
      {
        if (_fetch_current_file_index == -1)
        {
          ELLE_DEBUG("Thread %s has nothing to do, exiting", id);
          break; // some other thread figured out this was over
        }
        ELLE_DEBUG("Reading buffer at %s in mode %s", _fetch_current_position,
          explicit_ack? "read_encrypt_ack" :
           strong_encryption? "read_encrypt" : "encrypt"
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
        infinit::cryptography::Code code(
           explicit_ack ?
            source.encrypted_read_acknowledge(local_index,
                                              local_position, chunk_size,
                                              this->_snapshot->progress())
            : strong_encryption ?
              source.encrypted_read(local_index, local_position, chunk_size)
              :  source.read(local_index, local_position, chunk_size));
        elle::Buffer buffer;
        try
        {
          buffer = key.decrypt<elle::Buffer>(code);
        }
        catch(...)
        {
          ELLE_WARN("%s: decryption error on block %s/%s", *this, local_index, local_position);
          throw;
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
    ReceiveMachine::_disk_thread(Source& source, elle::Version peer_version,
                                 size_t chunk_size)
    {
      // somebody initialized our _store_expected_ state
      ELLE_TRACE_SCOPE("%s: start writing blocks to disk", *this);
      // Cached current transfer info to avoid refetching each time.
      // it should be there, but a logic error that gives us a
      // nothing-to-do-on-this-first-file state is possible and nonfatal
      // so do not use at
      boost::filesystem::ofstream* current_stream
        = _transfer_stream_map[_store_expected_file].get();
      FileSize current_file_full_size;
      boost::filesystem::path current_file_full_path;
      {
        frete::TransferSnapshot::File& f = _snapshot->file(_store_expected_file);
        current_file_full_size = f.size();
        current_file_full_path = f.full_path();
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
          ELLE_TRACE("%s: receiver got data for file %s at position %s with size %s, "
                     "will write to %s",
                     *this,
                     data.file_index, data.start_position,
                     data.buffer.size(),
                     current_file_full_path);
          ELLE_ASSERT(current_stream);
          // If this assert fails, packets were received out of order.
          ELLE_ASSERT_EQ(_store_expected_file, data.file_index);
          ELLE_ASSERT_EQ(_store_expected_position, data.start_position);
          boost::system::error_code ec;
          auto size = boost::filesystem::file_size(current_file_full_path, ec);
          if (ec)
          {
            ELLE_ERR("%s: destination file deleted: %s", *this, ec);
            throw elle::Exception(elle::sprintf("destination file %s deleted",
                                  current_file_full_path));
          }
          if (size != _store_expected_position)
          {
            ELLE_ERR(
              "%s: expected file size %s and actual file size %s are different",
              *this,
              _store_expected_position,
              size);
          throw elle::Exception("destination file corrupted");
          }
          // Write the file.
          ELLE_DUMP("content: %x (%sB)", buffer, buffer.size());
          current_stream->write((char const*) buffer.contents(), buffer.size());
          current_stream->flush();
          this->_snapshot->file_progress_increment(_store_expected_file, buffer.size());
          // OLD clients need this RPC to update progress
          if (peer_version < elle::Version(0, 8, 7))
            source.set_progress(this->_snapshot->progress());
          // Write snapshot state to file
          ELLE_DEBUG("%s: write down snapshot", *this)
          {
            ELLE_DUMP("%s: snapshot: %s", *this, *this->_snapshot);
            this->_save_transfer_snapshot();
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
              throw elle::Exception("transfer size mismatch");
            }
            // cleanup transfer data
            _transfer_stream_map.erase(_store_expected_file);
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
            if (_store_expected_position != current_file_full_size)
            { // We need blocks for that one, good
              current_stream = _transfer_stream_map.at(_store_expected_file).get();
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
    ReceiveMachine::_save_transfer_snapshot()
    {
      elle::serialize::to_file(this->_snapshot_path.string())
        << *this->_snapshot;
    }

    std::string
    ReceiveMachine::type() const
    {
      return "ReceiveMachine";
    }

    void
    ReceiveMachine::cleanup()
    { // our _get knows when it's finished, nothing to do
      ELLE_TRACE_SCOPE("%s: cleaning up", *this);
      // FIXME: cloud-check instead of relying if we have bufferer locally
      if (_bufferer)
        _bufferer->cleanup();
      // FIXME: cleanup raw cloud data
    }

    void
    ReceiveMachine::_cloud_synchronize()
    {
      this->current_state(TransactionMachine::State::Transfer);
      this->_cloud_operation();
    }

    void
    ReceiveMachine::IndexedBuffer::operator=(IndexedBuffer&&b)
    {
      buffer = std::move(b.buffer);
      start_position = b.start_position;
      file_index = b.file_index;
    }

    ReceiveMachine::IndexedBuffer::IndexedBuffer(elle::Buffer && b,
                                                 FileSize pos, FileID index)
    : buffer(std::move(b))
    , start_position(pos)
    , file_index(index)
    {}

    ReceiveMachine::IndexedBuffer::IndexedBuffer(IndexedBuffer && b)
    : buffer(std::move(b.buffer))
    , start_position(b.start_position)
    , file_index(b.file_index)
    {
    }
  }
}
