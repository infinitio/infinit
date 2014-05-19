#include <surface/gap/SendMachine.hh>

#include <boost/filesystem.hpp>

#include <elle/archive/zip.hh>
#include <elle/container/vector.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/system/system.hh>

#include <reactor/thread.hh>
#include <reactor/exception.hh>

#include <aws/Credentials.hh>
#include <aws/S3.hh>

#include <common/common.hh>
#include <papier/Identity.hh>
#include <station/Station.hh>

ELLE_LOG_COMPONENT("surface.gap.SendMachine");

namespace surface
{
  namespace gap
  {
    using TransactionStatus = infinit::oracles::Transaction::Status;

    // Common factored constructor.
    SendMachine::SendMachine(Transaction& transaction,
                             uint32_t id,
                             std::vector<std::string> files,
                             std::shared_ptr<Data> data)
      : Super(transaction, id, std::move(data))
      , _create_transaction_state(
        this->_machine.state_make(
          "create transaction", std::bind(&SendMachine::_create_transaction, this)))
      , _files(files)
    {
      if (this->files().empty())
        throw Exception(gap_no_file, "no files to send");
      // Cancel.
      this->_machine.transition_add(this->_create_transaction_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->canceled()}, true);
      // Exception.
      this->_machine.transition_add_catch(
        this->_create_transaction_state, this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            ELLE_WARN("%s: error while waiting for accept: %s",
                      *this, elle::exception_string(e));
          });
      this->_machine.state_changed().connect(
        [this] (reactor::fsm::State& state)
        {
          ELLE_LOG_COMPONENT("surface.gap.SendMachine.State");
          ELLE_TRACE("%s: entering %s", *this, state);
        });
      this->_machine.transition_triggered().connect(
        [this] (reactor::fsm::Transition& transition)
        {
          ELLE_LOG_COMPONENT("surface.gap.SendMachine.Transition");
          ELLE_TRACE("%s: %s triggered", *this, transition);
        });
    }

    SendMachine::~SendMachine()
    {}

    static std::streamsize const chunk_size = 1 << 18;

    void
    SendMachine::_ghost_cloud_upload()
    {
      // exit information for factored metrics writer.
      // needs to stay out of the try, as catch clause will fill those
      auto start_time = boost::posix_time::microsec_clock::universal_time();
      metrics::TransferExitReason exit_reason = metrics::TransferExitReasonUnknown;
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
                                         metrics::TransferMethodGhostCloud,
                                         duration,
                                         total_bytes_transfered,
                                         exit_reason,
                                         exit_message);
          }
        });
      try
      {
        typedef frete::Frete::FileSize FileSize;
        this->gap_state(gap_transaction_transferring);
        ELLE_TRACE_SCOPE("%s: ghost_cloud_upload", *this);
        typedef boost::filesystem::path path;
        path source_file_path;
        FileSize source_file_size;
        if (this->files().size() > 1)
        {
          // Our users might not appreciate downloading zillion of files from
          // their browser: make an archive make an archive name from data
          path archive_name = archive_info().first;
          // Use transfer data information to archive the files. This is what
          // was passed by the user, and what we will flatten.  That way if user
          // selects a directory it will be preserved.
          std::vector<boost::filesystem::path> sources(
            this->_files.begin(),
            this->_files.end());
          auto tmpdir = boost::filesystem::temp_directory_path() / transaction_id();
          boost::filesystem::create_directories(tmpdir);
          path archive_path = path(tmpdir) / archive_name;
          if (!exists(archive_path) || !this->transaction().archived())
          {
            ELLE_DEBUG("%s: archiving transfer files into %s", *this, archive_path);
            elle::archive::zip(sources, archive_path, [](boost::filesystem::path const& path)
              {
                std::string p(path.string());
                // Check if p maches our renaming scheme.
                size_t pos_beg = p.find_last_of('(');
                size_t pos_end = p.find_last_of(')');
                if (pos_beg != p.npos && pos_end != p.npos &&
                  (pos_end == p.size()-1 || p[pos_end+1] == '.'))
                {
                  try
                  {
                    std::string sequence =  p.substr(pos_beg + 1, pos_end-pos_beg-1);
                    unsigned int v = boost::lexical_cast<unsigned int>(sequence);
                    std::string result = p.substr(0, pos_beg+1)
                      + boost::lexical_cast<std::string>(v+1)
                      + p.substr(pos_end);
                    return result;
                  }
                  catch(const boost::bad_lexical_cast& blc)
                  {
                    // Go on.
                  }
                }
                return path.stem().string() + " (1)" + path.extension().string();
              });
            this->transaction().archived(true);
          }
          else
          {
            ELLE_DEBUG("%s: archive already present at %s", *this, archive_path);
          }
          source_file_path = archive_path;
        }
        else
        {
          source_file_path = *this->_files.begin();
        }
        source_file_size = boost::filesystem::file_size(source_file_path);
        std::string source_file_name = source_file_path.filename().string();
        ELLE_TRACE("%s: will ghost-cloud-upload %s of size %s",
                   *this, source_file_path, source_file_size);

        aws::S3 handler(this->make_aws_credentials_getter());

        typedef frete::Frete::FileSize FileSize;

        // AWS constraints: no more than 10k chunks, at least 5Mo block size
        FileSize chunk_size = std::max(FileSize(5*1024*1024), source_file_size / 9500);
        int chunk_count = source_file_size / chunk_size + ((source_file_size % chunk_size)? 1:0);
        ELLE_TRACE("%s: using chunk size of %s, with %s chunks",
                   *this, chunk_size, chunk_count);
        // Load our own snapshot that contains the upload uid
        std::string raw_snapshot_path = common::infinit::frete_snapshot_path(
          this->data()->sender_id,
          this->data()->id + "_raw");
        std::string upload_id;
        std::ifstream ifs(raw_snapshot_path);
        ifs >> upload_id;
        ELLE_DEBUG("%s: tried to reload id from %s, got %s",
                   *this, raw_snapshot_path, upload_id);
        std::vector<aws::S3::MultiPartChunk> chunks;
        int next_chunk = 0;
        int max_check_id = 0; // check for chunk presence up to that id
        int start_check_index = 0; // check for presence from that chunks index
        if (upload_id.empty())
        {
          //FIXME: pass correct mime type for non-zip case
          upload_id = handler.multipart_initialize(source_file_name);
          std::ofstream ofs(raw_snapshot_path);
          ofs << upload_id;
          ELLE_DEBUG("%s: saved id %s to %s",
                     *this, upload_id, raw_snapshot_path);
        }
        else
        { // Fetch block list
          chunks = handler.multipart_list(source_file_name, upload_id);
          std::sort(chunks.begin(), chunks.end(),
            [](aws::S3::MultiPartChunk const& a, aws::S3::MultiPartChunk const& b)
            {
              return a.first < b.first;
            });
          if (!chunks.empty())
          {
            // We expect missing blocks potentially, but only at the end
            //, ie we expect contiguous blocks 0 to (num_blocks - pipeline_size)
            for (int i=0; i<int(chunks.size()); ++i)
              if (chunks[i].first != i)
                break;
              else
                next_chunk = i+1;
            start_check_index = next_chunk;
            max_check_id = chunks.back().first;
          }
          ELLE_DEBUG("Will resume at chunk %s", next_chunk);
        }
        if (auto& mr = state().metrics_reporter())
        {
          auto now = boost::posix_time::microsec_clock::universal_time();
          mr->transaction_transfer_begin(
            this->transaction_id(),
            infinit::metrics::TransferMethodGhostCloud,
            float((now - start_time).total_milliseconds()) / 1000.0f);
        }
        // start pipelined upload
        auto pipeline_upload = [&, this](int id)
        {
          while (true)
          {
            // fetch a chunk number
            if (next_chunk >= chunk_count)
              return;
            int local_chunk = next_chunk++;
            if (local_chunk <= max_check_id)
            { // maybe we have it
              bool has_it = false;
              for (unsigned i=start_check_index;
                   i<chunks.size() && chunks[i].first <= max_check_id;
                   ++i)
              {
                if (chunks[i].first == local_chunk)
                {
                  has_it = true;
                  break;
                }
              }
              if (has_it)
                continue;
            }
            // upload it
            ELLE_DEBUG("%s: uploading chunk %s", *this, local_chunk);
            auto buffer = elle::system::read_file_chunk(
              source_file_path,
              local_chunk*chunk_size, chunk_size);
            std::string etag = handler.multipart_upload(
              source_file_name, upload_id,
              buffer,
              local_chunk);
            // FIXME
            // auto progress = float(next_chunk) / float(chunk_count);
            // Now, totally fake progress on the original frete by
            // updating the global progress, and not individual files
            // progress. That way we don't produce fake snapshot state data
            // for further cloud upload.
            total_bytes_transfered += buffer.size();
            chunks.push_back(std::make_pair(local_chunk, etag));
          }
        };
        int num_threads = 4;
        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          for (int i=0; i<num_threads; ++i)
            scope.run_background(elle::sprintf("cloud %s", i),
                                 std::bind(pipeline_upload, i));
          scope.wait();
        };
        std::sort(chunks.begin(), chunks.end(),
          [](aws::S3::MultiPartChunk const& a, aws::S3::MultiPartChunk const& b)
            {
              return a.first < b.first;
            });
        handler.multipart_finalize(source_file_name, upload_id, chunks);
        // mark transfer as raw-finished
        this->gap_state(gap_transaction_finished);
        this->finished().open();
        exit_reason = metrics::TransferExitReasonFinished;
      } // try
      catch(reactor::Terminate const&)
      {
        exit_reason = metrics::TransferExitReasonTerminated;
        throw;
      }
      catch(...)
      {
        exit_reason = metrics::TransferExitReasonError;
        exit_message = elle::exception_string();
        throw;
      }
    }

    void
    SendMachine::cleanup()
    {
      if (this->data()->id.empty())
      { // Early failure, no transaction_id -> nothing to clean up
        return;
      }
      // clear temporary session directory
      std::string tid = transaction_id();
      ELLE_ASSERT(!tid.empty());
      ELLE_ASSERT(tid.find('/') == tid.npos);
      auto tmpdir = boost::filesystem::temp_directory_path() / tid;
      ELLE_LOG("%s: clearing temporary directory %s",
               *this, tmpdir);
      boost::filesystem::remove_all(tmpdir);
    }

    std::pair<std::string, bool>
    SendMachine::archive_info()
    {
      auto const& files = this->files();
      if (files.size() == 1)
      {
        boost::filesystem::path file(*files.begin());
        if (is_directory(status(file)))
          return std::make_pair(
            file.filename().replace_extension("zip").string(), true);
        else
          return std::make_pair(file.filename().string(), false);
      }
      else
        return std::make_pair("archive.zip", true);
    }
  }
}
