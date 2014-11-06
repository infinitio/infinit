#include <surface/gap/SendMachine.hh>

#include <boost/filesystem.hpp>

#include <elle/archive/archive.hh>
#include <elle/container/vector.hh>
#include <elle/container/map.hh>
#include <elle/os/environ.hh>
#include <elle/os/path.hh>
#include <elle/system/system.hh>

#include <reactor/thread.hh>
#include <reactor/exception.hh>

#include <aws/Credentials.hh>
#include <aws/Exceptions.hh>
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

    /*-------------.
    | Construction |
    `-------------*/

    // Constructor for when transaction was started on another device.
    SendMachine::SendMachine(Transaction& transaction,
                             uint32_t id,
                             std::shared_ptr<Data> data)
      : Super(transaction, id, std::move(data))
      , _plain_progress(0)
      , _create_transaction_state(
        this->_machine.state_make(
          "create transaction",
          std::bind(&SendMachine::_create_transaction, this)))
    {
      // Cancel.
      this->_machine.transition_add(this->_create_transaction_state,
                                    this->_cancel_state,
                                    reactor::Waitables{&this->canceled()}, true);
    }

    // Constructor for sender device.
    SendMachine::SendMachine(Transaction& transaction,
                             uint32_t id,
                             std::vector<std::string> files,
                             std::shared_ptr<Data> data)
      : SendMachine(transaction, id, data)
    {
      this->_files = files;
      if (this->files().empty())
        throw Exception(gap_no_file, "no files to send");
      this->_machine.transition_add_catch_specific<
        boost::filesystem::filesystem_error>(this->_create_transaction_state,
                                             this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            this->transaction().failure_reason(elle::exception_string(e));
            ELLE_WARN("%s: filesystem error while creating transaction: %s",
                      *this, elle::exception_string(e));
          });
      // Exception.
      this->_machine.transition_add_catch(
        this->_create_transaction_state, this->_fail_state)
        .action_exception(
          [this] (std::exception_ptr e)
          {
            this->transaction().failure_reason(elle::exception_string(e));
            ELLE_WARN("%s: error while creating transaction: %s",
                      *this, elle::exception_string(e));
          });
    }

    bool
    SendMachine::concerns_this_device()
    {
      if (this->state().device().id == this->data()->sender_device_id)
        return true;
      else
        return false;
    }

    SendMachine::~SendMachine()
    {}

    /*-------------.
    | Plain upload |
    `-------------*/

    float
    SendMachine::progress() const
    {
      auto progress = this->_plain_progress;
      for (auto const& chunk: this->_plain_progress_chunks)
        progress += chunk.second;
      return progress;
    }

    class S3
      : public aws::S3
    {
    public:
      S3(State& state,
         std::function<aws::Credentials(bool)> query_credentials)
        : aws::S3(query_credentials)
        , _state(state)
      {}

      virtual
      std::string
      hostname(aws::Credentials const& credentials) const override
      {
        auto replace = this->_state.s3_hostname();
        if (!replace)
          return aws::S3::hostname(credentials);
        else
          return *replace;
      }

      ELLE_ATTRIBUTE(State&, state);
    };

    void
    SendMachine::_plain_upload()
    {
      static const std::unordered_map<std::string, std::string> mimes = {
      {"mp3", "audio/mpeg"},
      {"wav", "audio/wav"},
      {"mp4", "audio/mp4"},
      {"m4a", "audio/mp4"},
      {"aac", "audio/mp4"},
      {"ogg", "audio/ogg"},
      {"oga", "audio/ogg"},
      {"ogv", "video/ogg"},
      {"webm", "video/webm"},
      {"avi", "video/avi"},
      {"mpg", "video/mpeg"},
      {"mpeg", "video/mpeg"},
      {"m4v", "video/mp4"},
      };
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
        this->gap_status(gap_transaction_transferring);
        ELLE_TRACE_SCOPE("%s: start plain upload", *this);
        typedef boost::filesystem::path path;
        path source_file_path;
        FileSize file_size;
        auto archive = this->archive_info();
        ELLE_DEBUG("archive: %s", archive);
        if (archive.second)
        {
          // Our users might not appreciate downloading zillion of files from
          // their browser: make an archive make an archive name from data
          path archive_name = archive.first;
          // Use transfer data information to archive the files. This is what
          // was passed by the user, and what we will flatten.  That way if user
          // selects a directory it will be preserved.
          std::vector<boost::filesystem::path> sources(
            this->_files.begin(),
            this->_files.end());
          path archive_path =
            this->transaction().snapshots_directory() / "archive";
          boost::filesystem::create_directories(archive_path);
          archive_path /= archive_name;
          if (!exists(archive_path) || !this->transaction().archived())
          {
            ELLE_DEBUG("%s: archiving transfer files into %s", *this, archive_path);
            auto renaming_callback =
              [](boost::filesystem::path const& path)
              { // File renaming callback
                std::string p(path.string());
                // Check if p matches our renaming scheme.
                size_t pos_beg = p.find_last_of('(');
                size_t pos_end = p.find_last_of(')');
                if (pos_beg != p.npos && pos_end != p.npos &&
                  (pos_end == p.size()-1 || p[pos_end+1] == '.'))
                {
                  try
                  {
                    std::string sequence = p.substr(pos_beg + 1, pos_end-pos_beg-1);
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
              };
            ELLE_TRACE("%s: begin archiving thread", *this);
            reactor::background(
              [&]
              {
                int64_t max_compress_size =
                  this->transaction().state().configuration().max_compress_size;
                if (max_compress_size == 0)
                  max_compress_size = 10*1000*1000;
                bool compress = this->_total_size <= max_compress_size;
                elle::archive::archive(
                  compress ? elle::archive::Format::zip
                           : elle::archive::Format::zip_uncompressed,
                  sources, archive_path, renaming_callback);
              });
            ELLE_TRACE("%s: join archiving thread", *this)
              this->transaction().archived(true);
          }
          else
          {
            ELLE_DEBUG("%s: archive already present at %s", *this, archive_path);
          }
          source_file_path = archive_path;
        }
        else
          source_file_path = *this->_files.begin();
        file_size = boost::filesystem::file_size(source_file_path);
        std::string source_file_name = source_file_path.filename().string();
        ELLE_TRACE("%s: will ghost-cloud-upload %s of size %s",
                   *this, source_file_path, file_size);
        auto get_credentials = [this] (bool first_time)
          {
            return this->_aws_credentials(first_time);
          };
        S3 handler(this->transaction().state(), get_credentials);
        handler.on_error([&](aws::AWSException const& exception, bool will_retry)
          {
            this->_report_s3_error(exception, will_retry);
          });
        typedef frete::Frete::FileSize FileSize;
        auto const& config = this->transaction().state().configuration();
        // AWS constraints: no more than 10k chunks, at least 5Mo block size
        FileSize default_chunk_size(
          std::max(config.s3.multipart_upload.chunk_size, 5 * 1024 * 1024));
        FileSize chunk_size = std::max(default_chunk_size, file_size / 9500);
        int chunk_count =
          file_size / chunk_size + ((file_size % chunk_size) ? 1 : 0);
        ELLE_TRACE("%s: using chunk size of %s, with %s chunks",
                   *this, chunk_size, chunk_count);
        std::vector<aws::S3::MultiPartChunk> chunks;
        int next_chunk = 0;
        int max_check_id = 0; // check for chunk presence up to that id
        int start_check_index = 0; // check for presence from that chunks index
        std::string upload_id;
        if (this->transaction().plain_upload_uid())
        {
          ELLE_DEBUG("trying to resume with existing upload id: %s",
                     this->transaction().plain_upload_uid());
          try
          {
            ELLE_DEBUG("fetch block list");
            upload_id = *this->transaction().plain_upload_uid();
            chunks = handler.multipart_list(source_file_name, upload_id);
            std::sort(chunks.begin(), chunks.end(),
              [](aws::S3::MultiPartChunk const& a, aws::S3::MultiPartChunk const& b)
              {
                return a.first < b.first;
              });
            ELLE_DEBUG("chunks: %s", chunks);
            if (!chunks.empty())
            {
              // We expect missing blocks potentially, but only at the end
              //, ie we expect contiguous blocks 0 to (num_blocks - pipeline_size)
              for (int i = 0; i < int(chunks.size()); ++i)
                if (chunks[i].first != i)
                  break;
                else
                  next_chunk = i+1;
              start_check_index = next_chunk;
              max_check_id = chunks.back().first;
            }
            else
            {
              ELLE_DEBUG("no chunks found");
            }
            ELLE_DEBUG("will resume at chunk %s", next_chunk);
          }
          catch (aws::AWSException const& e)
          {
            // Error with Code=NoSuchUpload ?
            ELLE_WARN("%s: Failed to resume upload: %s", *this, e.what());
            this->transaction().plain_upload_uid(boost::optional<std::string>());
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          ELLE_ASSERT_NO_OTHER_EXCEPTION
        }
        ELLE_DEBUG("plain upload uid: %s", transaction().plain_upload_uid());
        if (!this->transaction().plain_upload_uid()) // Not else! Code above can reset plain_upload_uid
        {
          ELLE_DEBUG("plain upload uid still empty");
          // Pass correct mime type for known extensions
          std::string mime_type("binary/octet-stream");
          std::string extension = boost::filesystem::path(source_file_name)
            .extension().string();
          if (!extension.empty())
            extension = extension.substr(1);
          auto it = mimes.find(extension);
          if (it != mimes.end())
            mime_type = it->second;
          upload_id = handler.multipart_initialize(source_file_name, mime_type);
          this->transaction().plain_upload_uid(upload_id);
          this->transaction()._snapshot_save();
          ELLE_TRACE("%s: saved upload ID %s to snapshot",
                     *this, *this->transaction().plain_upload_uid());
        }
        ELLE_DEBUG("next chunk to be uploaded: %s", next_chunk);
        auto chunk_uploaded = next_chunk;
        this->_plain_progress =
          float(chunk_uploaded) / float(chunk_count);
        if (auto& mr = state().metrics_reporter())
        {
          auto now = boost::posix_time::microsec_clock::universal_time();
          mr->transaction_transfer_begin(
            this->transaction_id(),
            infinit::metrics::TransferMethodGhostCloud,
            float((now - start_time).total_milliseconds()) / 1000.0f);
        }
        // start pipelined upload
        elle::system::FileHandle file(source_file_path,
                                      elle::system::FileHandle::READ);
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
              for (unsigned i = start_check_index;
                   i < chunks.size() && chunks[i].first <= max_check_id;
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
            auto buffer = file.read((int64_t)local_chunk * (int64_t)chunk_size, chunk_size);
            auto size = buffer.size();
            if (size != chunk_size  && local_chunk != chunk_count -1)
              ELLE_WARN("%s: chunk %s/%s is too small: %s bytes",
                        *this, local_chunk, chunk_count, size);
            this->_plain_progress_chunks[local_chunk] = 0;
            std::string etag = handler.multipart_upload(
              source_file_name, upload_id,
              buffer,
              local_chunk,
              std::function<void (int)>(
                [this, size, local_chunk, chunk_count] (int uploaded)
                {
                  this->_plain_progress_chunks[local_chunk] =
                    float(uploaded) / size / chunk_count;
                }));
            if (etag.empty())
              ELLE_WARN("%s: upload of chunk %s/%s gave empty etag",
                        *this, local_chunk, chunk_count);
            ++chunk_uploaded;
            this->_plain_progress =
              float(chunk_uploaded) / float(chunk_count);
            total_bytes_transfered += size;
            chunks.push_back(std::make_pair(local_chunk, etag));
            this->_plain_progress_chunks.erase(local_chunk);
          }
        };
        int num_threads = config.s3.multipart_upload.parallelism;
        ELLE_DEBUG("using %s parallel threads", num_threads);
        std::string env_num_threads =
          elle::os::getenv("INFINIT_NUM_PLAIN_UPLOAD_THREAD", "");
        if (!env_num_threads.empty())
          num_threads = boost::lexical_cast<unsigned>(env_num_threads);
        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          for (int i = 0; i < num_threads; ++i)
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
        // Let the finished state update gap and meta.
        this->finished().open();
        exit_reason = metrics::TransferExitReasonFinished;
      } // try
      catch (boost::filesystem::filesystem_error const& e)
      {
        exit_message = e.what();
        exit_reason = infinit::metrics::TransferExitReasonError;
        if (e.code() == boost::system::errc::no_such_file_or_directory)
        {
          ELLE_WARN("%s: source file disappeared, cancel : %s",
                    *this, e.what());
          this->cancel("source file missing");
          throw;
        }
        else
        {
          ELLE_WARN("%s: source file corrupted (%s), cancel",
                    *this, e.what());
          this->cancel(elle::sprintf("source file error: %s", e.what()));
          throw;
        }
      }
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
      ELLE_TRACE_SCOPE("%s: cleanup", *this);
      if (this->data()->id.empty())
      { // Early failure, no transaction_id -> nothing to clean up
        return;
      }
      auto base = this->transaction().snapshots_directory();
      // clear generated files that may have been generated for the transfer

      // It is possible for files to be copied without write permissions, as
      // attribute are preserved by bfs::copy.
      elle::os::path::force_write_permissions(base / "mirror_files");
      boost::filesystem::remove_all(base / "mirror_files");
      elle::os::path::force_write_permissions(base / "archive");
      boost::filesystem::remove_all(base / "archive");
    }

    std::pair<std::string, bool>
    SendMachine::archive_info()
    {
      auto const& files = this->files();
      if (files.size() == 1)
      {
        boost::filesystem::path file(*files.begin());
        if (is_directory(status(file)))
        {
          // check for file name of the form '.foo'
          if (file.filename().extension() == file.filename())
            return std::make_pair(
              file.filename().string().substr(1) + ".zip",
              true);
          else
            return std::make_pair(
              file.filename().replace_extension("zip").string(), true);
        }
        else
          return std::make_pair(file.filename().string(), false);
      }
      else
        return std::make_pair(elle::sprintf("%s files.zip", files.size()),
                              true);
    }

    void
    SendMachine::try_mirroring_files(frete::Frete::FileSize total_size)
    {
      frete::Frete::FileSize max_mirror_size
        = transaction().state().configuration().max_mirror_size;
      if (!max_mirror_size)
        max_mirror_size = 100 * 1000 * 1000; // 2s copy on 50Mb/s hard drive
      if (total_size >= max_mirror_size)
      {
        ELLE_TRACE("%s: Not mirroring, total size %s above configured maximum %s",
                   *this,
                   total_size,
                   max_mirror_size);
        return;
      }
      // Check disk space
      boost::system::error_code erc;
      auto space = boost::filesystem::space(
        this->transaction().snapshots_directory(), erc);
      if (erc)
      {
        ELLE_TRACE("%s: Not mirroring, failed to get free space: %s",
                   *this,
                   erc);
        return;
      }
      if (space.available < total_size * 110 /100)
      {
        ELLE_TRACE("%s: not mirroring, free disk space of %s too low for %s",
                   *this,
                   space.available,
                   total_size);
        return;
      }
      // If you change it, also change in crash reporter filter
      static const char* mirror_dir = "mirror_files"; /* read above!*/
      boost::filesystem::path mirror_path =
        this->transaction().snapshots_directory() / mirror_dir;
      bool validate = false; // set at
      elle::SafeFinally maybe_cleanup([&] {
          if (validate)
            return;
          ELLE_LOG("%s: File mirroring failure, cleaning up.", *this);
          boost::filesystem::remove_all(mirror_path, erc);
      });
      ELLE_TRACE("%s: trying to mirror files (%s < %s) to %s",
                 *this, total_size, max_mirror_size, mirror_path);
      boost::filesystem::create_directories(mirror_path, erc);
      if (erc)
      {
        ELLE_TRACE("%s: Failed to create mirror directory", *this);
        return;
      }
      std::vector<std::string> moved_files;
      for (auto const& f: this->_files)
      {
        namespace bfs = boost::filesystem;
        auto target = mirror_path / boost::filesystem::path(f).filename();
        if (bfs::is_directory(f))
        {
          boost::filesystem::copy(f, target, erc);
          if (erc)
          {
            ELLE_TRACE("%s: Error while copying %s: %s",
                       *this, target, erc);
            return; // Finally will clean up
          }
          auto it = bfs::recursive_directory_iterator(f);
          for (;it != bfs::recursive_directory_iterator(); ++it)
          {
            // Construct target filename
            bfs::path p(*it);
            std::vector<bfs::path> components;
            // level=0: we still push directory name, and file name, hence +2
            for (int i=0; i<=it.level()+1; ++i, p=p.parent_path())
            {
              components.push_back(p.filename());
            }
            bfs::path target(mirror_path);
            for (unsigned i=0; i<components.size(); ++i)
              target /= components[components.size()-i-1];
            ELLE_DEBUG("Copying %s -> %s", *it, target);
            // Use copy for both files and directories, it works
            // And it creates parent directories before child files
            if (bfs::is_symlink(*it))
              bfs::create_symlink(read_symlink(*it), target, erc);
            else
              bfs::copy(*it, target, erc);
            if (erc)
            {
              ELLE_TRACE("%s: Error while copying %s: %s", *this, *it, erc);
              return; // Finally will clean up
            }
          }
        }
        else
          boost::filesystem::copy(f, target, erc);
        if (erc)
        {
          ELLE_TRACE("%s: Error while copying %s: %s", *this, f, erc);
          return; // Finally will clean up
        }
        ELLE_DEBUG("%s: Replacing %s with %s", *this, f, target);
        moved_files.push_back(target.string());
      }
      validate = true;
      this->_files = moved_files;
      ELLE_TRACE("%s: Mirroring successful", *this);
    }
  }
}
