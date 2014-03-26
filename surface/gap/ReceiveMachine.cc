#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <elle/finally.hh>
#include <elle/os/environ.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>

#include <reactor/exception.hh>
#include <reactor/thread.hh>
#include <reactor/Channel.hh>

#include <common/common.hh>

#include <frete/Frete.hh>
#include <frete/TransferSnapshot.hh>

#include <papier/Identity.hh>

#include <station/Station.hh>

#include <surface/gap/FilesystemTransferBufferer.hh>
#include <surface/gap/ReceiveMachine.hh>
#include <surface/gap/Rounds.hh>

#include <version.hh>


ELLE_LOG_COMPONENT("surface.gap.ReceiveMachine");

namespace surface
{
  namespace gap
  {
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
                                   bool):
      TransactionMachine(state, id, std::move(data)),
      _wait_for_decision_state(
        this->_machine.state_make(
          "wait for decision", std::bind(&ReceiveMachine::_wait_for_decision, this))),
      _accept_state(
        this->_machine.state_make(
          "accept", std::bind(&ReceiveMachine::_accept, this))),
      _accepted("accepted barrier"),
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

      if (boost::filesystem::exists(this->_snapshot_path))
      {
        ELLE_LOG("snapshot exist at %s", this->_snapshot_path);
        try
        {
          elle::SafeFinally delete_snapshot{
            [&]
            {
              try
              {
                boost::filesystem::remove(this->_snapshot_path);
              }
              catch (std::exception const&)
              {
                ELLE_ERR("couldn't delete snapshot at %s: %s",
                         this->_snapshot_path, elle::exception_string());
              }
            }};

          this->_snapshot.reset(
            new frete::TransferSnapshot(
              elle::serialize::from_file(this->_snapshot_path.string())));
        }
        catch (std::exception const&) //XXX: Choose the right exception here.
        {
          ELLE_ERR("%s: snap shot was invalid: %s", *this, elle::exception_string());
        }
      }
    }

    ReceiveMachine::ReceiveMachine(surface::gap::State const& state,
                                   uint32_t id,
                                   TransactionMachine::State const current_state,
                                   std::shared_ptr<TransactionMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
    {
      ELLE_TRACE_SCOPE("%s: construct from data %s, starting at %s",
                       *this, *this->data(), current_state);

      switch (current_state)
      {
        case TransactionMachine::State::NewTransaction:
          //
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
                                   std::shared_ptr<TransactionMachine::Data> data):
      ReceiveMachine(state, id, std::move(data), true)
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
      bool exists = boost::filesystem::exists(start_point / first);
      ELLE_DEBUG("Looking for a replacment name for %s, firstcomp=%s, exists=%s", path, first, exists);
      if (! exists)
      { // we will create the path along the way so we must add itself into mapping
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
            this->finished().open();
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
      try
      {
        ELLE_DEBUG("%s: create cloud bufferer", *this);
        FilesystemTransferBufferer bufferer(*this->data(),
                                            "/tmp/infinit-buffering");
        ELLE_DEBUG("%s: download from the cloud", *this)
         this->get(bufferer);
      }
      catch (TransferBufferer::DataExhausted const&)
      {
        ELLE_TRACE("%s: Data exhausted on cloud bufferer", *this);
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
      boost::filesystem::path output_path(this->state().output_dir());
      auto count = source.count();

      // total_size can be 0 if all files are empty.
      auto total_size = source.full_size();

      if (this->_snapshot != nullptr)
      {
        if ((this->_snapshot->total_size() != total_size) ||
            (this->_snapshot->count() != count))
        {
          ELLE_ERR("snapshot data (%s) are invalid", *this->_snapshot);
          throw elle::Exception("invalid transfer data");
        }
      }
      else
      {
        this->_snapshot.reset(new frete::TransferSnapshot(count, total_size));
      }

      static const std::streamsize chunk_size = rpc_chunk_size();

      ELLE_DEBUG("transfer snapshot: %s", *this->_snapshot);



      FileID last_index = this->_snapshot->count();
      if (last_index > 0)
        --last_index;

      auto infos = source.files_info();

      // reconstruct directory name mapping data so that files in transfer
      // but not yet in snapshot will reuse it
      for (int i = 0; i < this->_snapshot->count(); ++i)
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



      // Snapshot only has info on files for which transfer started,
      // and we transfer in order, so we know all files in snapshot except
      // maybe the last one are fully transfered.
      FileID current_index = last_index;
      TransferData * current_transfer = 0;
      // get first transfer for which there is something to do
      while (current_index < count)
      {
        TransferDataPtr next = _initialize_one(
          current_index,
          infos.at(current_index).first,
          infos.at(current_index).second,
          output_path,
          name_policy);
        if (next)
        {
          current_transfer = next.get();
          _transfer_data_map[current_index] = std::move(next);
          break;
        }
        ++current_index;
      }
      if (current_transfer)
      {
        size_t current_position = current_transfer->start_position;
        size_t current_full_size = current_transfer->tr.size();
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
          auto reader = [&,this](int id)
          {
            while (true)
            {
              if (!current_transfer)
              {
                ELLE_DEBUG("Thread %s has nothing to do, exiting", id);
                break; // some other thread figured out this was over
              }
              ELLE_DUMP("Reading buffer at %s", current_position);
              if (current_position >= current_full_size)
              {
                ELLE_DUMP("Thread %s would read past end", id);
                // switch to next file
                ++current_index;
                if (current_index >= count)
                {
                  // we're done
                  current_transfer = nullptr;
                  break;
                }
                {
                  auto transfer =
                    this->_initialize_one(current_index,
                                          infos.at(current_index).first,
                                          infos.at(current_index).second,
                                          output_path,
                                          name_policy);
                  current_transfer = transfer.get();
                  this->_transfer_data_map.insert(
                    std::make_pair(current_index, std::move(transfer)));
                }
                // technically for now current_position is always 0
                current_position = current_transfer->start_position;
                current_full_size = current_transfer->tr.size();
              }
              size_t next_read = current_position;
              current_position += chunk_size;

              FileID local_current_index = current_index;
              // This line blocks, no shared state access past that point!
              elle::Buffer buffer{
                key.decrypt<elle::Buffer>(
                  strong_encryption ?
                  source.encrypted_read(current_index, next_read, chunk_size) :
                  source.read(current_index, next_read , chunk_size))};
              ELLE_DUMP("Queuing buffer fileindex:%s offset:%s size:%s", local_current_index, next_read, buffer.size());
              this->_buffers.put(
                IndexedBuffer{std::move(buffer),
                              next_read, local_current_index});
            }
            ELLE_DEBUG("reader %s exiting cleanly", id);
          };
          for (int i = 0; i < num_reader; ++i)
            scope.run_background(elle::sprintf("transfer reader %s", i),
                                 std::bind(reader, i));
          reactor::Thread disk_writer(
            "receive writer",
            std::bind(&ReceiveMachine::_reader_thread<Source>,
                      this, std::ref(source),
                      peer_version, chunk_size));
          try
          {
            scope.wait();
            // tell the reader thread to terminate
            this->_buffers.put(
              IndexedBuffer{elle::Buffer(), FileSize(-1), FileID(-1)});
            reactor::wait(disk_writer);
            ELLE_TRACE("finish_transfer exited cleanly");
          }
          catch(...)
          {
            disk_writer.terminate_now();
            throw;
          }
        }; // scope
      }// if current_transfer
      // this->finished.open();
      ELLE_LOG("Transfer finished, peer is %s", peer_version);
      if (peer_version >= elle::Version(0, 8, 7))
      {
        ELLE_LOG("Notifying finish");
        source.finish();
      }

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

    ReceiveMachine::TransferDataPtr
    ReceiveMachine::_initialize_one(FileID index,
                                    const std::string& file_path,
                                    FileSize file_size,
                                    boost::filesystem::path output_path,
                                    const std::string& name_policy)
    {
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
        return TransferDataPtr();
      }
      TransferDataPtr transfer(new TransferData(tr, fullpath, tr.progress()));
      return std::move(transfer);
    }

    ReceiveMachine::TransferData::TransferData(
      frete::TransferSnapshot::File& tr,
      boost::filesystem::path full_path,
      FileSize start_position)
      : tr(tr)
      , full_path(full_path)
      , start_position(start_position)
    {
      boost::filesystem::create_directories(full_path.parent_path());
      output.open(full_path, std::ios::app | std::ios::binary);
    }

    template<typename Source>
    void
    ReceiveMachine::_reader_thread(Source& source, elle::Version peer_version,
                                   size_t chunk_size)
    {
      ELLE_TRACE_SCOPE("%s: start writing blocks to disk", *this);
      // Cached current transfer info to avoid refetching each time.
      TransferData* current_transfer = 0;
      size_t current_index = -1;
      // We do not store an expected transfer position, it is already
      // present in the snapshot and we assert on it.
      while (true)
      {
         IndexedBuffer data = _buffers.get();
         if (data.file_index == FileID(-1))
         {
           ELLE_DEBUG("%s: done writing blocks to disk", *this);
           break;
         }
         const elle::Buffer& buffer = data.buffer;
         size_t position = data.start_position;
         size_t index = data.file_index;
         if (current_index != index)
         {
           current_index = index;
           // We know it's there if we have a buffer for it, RPC requester
           // thread created it and we delete it.
           current_transfer = _transfer_data_map.at(index).get();
         }
         ELLE_TRACE("%s: receiver got data for file %s at position %s, "
                    "will write to %s",
                    *this,
                    index, position, current_transfer->full_path);
         ELLE_ASSERT(current_transfer);
         // If this assert fails, packets were received out of order.
         ELLE_ASSERT_EQ(position, current_transfer->tr.progress());
         ELLE_ASSERT_LT(index, this->_snapshot->count());
         boost::system::error_code ec;
         auto size = boost::filesystem::file_size(current_transfer->full_path, ec);
         if (ec)
         {
           ELLE_ERR("%s: destination file deleted: %s", *this, ec);
           throw elle::Exception(elle::sprintf("destination file %s deleted",
                                               current_transfer->full_path));
         }
         if (size != current_transfer->tr.progress())
         {
           ELLE_ERR(
             "%s: expected file size %s and actual file size %s are different",
             *this,
             current_transfer->tr.progress(),
             size);
           throw elle::Exception("destination file corrupted");
         }
         // Write the file.
         ELLE_DUMP("content: %x (%sB)", buffer, buffer.size());
         current_transfer->output.write((char const*) buffer.contents(), buffer.size());
         current_transfer->output.flush();
         this->_snapshot->file_progress_increment(index, buffer.size());
         // OLD clients need this RPC to update progress
         if (peer_version < elle::Version(0, 8, 7))
           source.set_progress(this->_snapshot->progress());
         // Write snapshot state to file
         ELLE_TRACE("%s: write down snapshot", *this)
         {
           ELLE_DUMP("%s: snapshot: %s", *this, *this->_snapshot);
           elle::serialize::to_file(this->_snapshot_path.string())
             << *this->_snapshot;
         }
         auto progress = current_transfer->tr.progress();
         auto file_size = current_transfer->tr.size();
         if (buffer.size() < chunk_size || progress >= file_size)
         {
           if (progress != file_size)
           {
             ELLE_ERR("%s: end of transfer with unexpected size, "
                      "got %s, expected %s",
                      *this, progress, file_size);
             throw elle::Exception("transfer size mismatch");
           }
           // cleanup transfer data
           current_transfer = 0;
           _transfer_data_map.erase(current_index);
           current_index = -1;
         }
      }
    }


    std::string
    ReceiveMachine::type() const
    {
      return "ReceiveMachine";
    }

    void
    ReceiveMachine::cleanup()
    { // our _get knows when it's finished, nothing to do
    }
  }
}
