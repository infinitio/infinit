#include <functional>
#include <ios>
#include <limits>
#include <algorithm>

#include <boost/filesystem/fstream.hpp>

#include <elle/archive/archive.hh>
#include <elle/AtomicFile.hh>
#include <elle/Error.hh>
#include <elle/container/map.hh>
#include <elle/finally.hh>
#include <elle/serialization/json/SerializerIn.hh>
#include <elle/serialization/json/SerializerOut.hh>
#include <elle/system/system.hh>

#include <cryptography/SecretKey.hh>
#include <cryptography/PrivateKey.hh>
#include <cryptography/Code.hh>

#include <reactor/network/socket.hh>

#include <frete/Frete.hh>
#include <frete/TransferSnapshot.hh>

#include <version.hh>

ELLE_LOG_COMPONENT("frete.Frete");

namespace frete
{
  /*-------------.
  | Construction |
  `-------------*/

  class Frete::Impl
  {
  public:
    Impl(std::string const& password)
      : _old_key(infinit::cryptography::cipher::Algorithm::aes256, password)
      , _key(new infinit::cryptography::SecretKey(
          infinit::cryptography::SecretKey::generate(
            infinit::cryptography::cipher::Algorithm::aes256, 2048)))
    {
      ELLE_DEBUG("frete impl initialized");
    }

    ELLE_ATTRIBUTE_R(infinit::cryptography::SecretKey, old_key);
    ELLE_ATTRIBUTE_R(std::unique_ptr<infinit::cryptography::SecretKey>, key);
    ELLE_ATTRIBUTE_R(std::unique_ptr<infinit::cryptography::PublicKey>, peer_key);
    friend class Frete;
  };

  Frete::Frete(std::string const& password,
               infinit::cryptography::KeyPair const& self_key,
               boost::filesystem::path const& snapshot_destination,
               boost::filesystem::path const& mirror_root,
               bool files_mirrored)
    : _impl(new Impl(password))
    , _mirror_root(mirror_root)
    , _finished()
    , _progress_changed("progress changed signal")
    , _transfer_snapshot()
    , _snapshot_destination(snapshot_destination)
  {
    if (exists(this->_snapshot_destination))
    {
      ELLE_TRACE_SCOPE("%s: load snapshot %s",
                       *this, this->_snapshot_destination);
      try
      {
        std::unique_ptr<TransferSnapshot> snapshot;
        {
          elle::AtomicFile file(this->_snapshot_destination);
          file.read() << [&] (elle::AtomicFile::Read& read)
          {
            elle::serialization::json::SerializerIn input(read.stream(), false);
            snapshot.reset(new TransferSnapshot(input));
          };
        }
        ELLE_DUMP("%s: Loaded snapshot %s with progress[0] %s",
                  *this,
                  this->_snapshot_destination,
                  (snapshot->file_count()?
                   snapshot->file(0).progress():
                   0));
        // reload key from snapshot
        _impl->_key.reset(
          new infinit::cryptography::SecretKey(
            self_key.k().decrypt<infinit::cryptography::SecretKey>(
              *snapshot->key_code())));
        this->_transfer_snapshot = std::move(snapshot);
      }
      catch (boost::filesystem::filesystem_error const&)
      {
        ELLE_TRACE("%s: unable to read snapshot file at %s: %s",
                   *this, this->_snapshot_destination, elle::exception_string());
      }
      catch (elle::Error const& e)
      {
        ELLE_WARN("%s: snapshot at %s is invalid: %s",
                  *this, this->_snapshot_destination, e);
      }
    }
    if (this->_transfer_snapshot == nullptr)
    {
      this->_transfer_snapshot.reset(new TransferSnapshot(files_mirrored));
      // Write encrypted session key to the snapshot
      this->_transfer_snapshot->set_key_code(
        self_key.K().encrypt(*_impl->_key));
      // immediately save the snapshot so that key never changes
      this->save_snapshot();
    }
  }

  void
  Frete::set_peer_key(infinit::cryptography::PublicKey peer_K)
  {
    if (this->_impl->_peer_key)
      throw elle::Exception("Peer key can only be set once");
    this->_impl->_peer_key.reset(
      new infinit::cryptography::PublicKey(std::move(peer_K)));
  }

  bool
  Frete::has_peer_key() const
  {
    return this->_impl->_peer_key != nullptr;
  }

  Frete::~Frete()
  {}

  void
  Frete::add(boost::filesystem::path const& path)
  {
    ELLE_TRACE("%s: add %s", *this, path);

    if (!boost::filesystem::exists(path))
      throw elle::Exception(elle::sprintf("given path %s doesn't exist", path));

    if (is_directory(path))
    {
      bool has_symlink = false;
      boost::filesystem::recursive_directory_iterator end;
      for (auto it = boost::filesystem::recursive_directory_iterator(path);
           it != end;
           ++it)
      {
        if (boost::filesystem::is_symlink(it->path()))
        {
          has_symlink = true;
          break;
        }
      }
      if (has_symlink)
      { // Replace with ziped version of itself
        boost::filesystem::path archive_name = path.filename();
        archive_name += ".zip";
        boost::filesystem::path archive_path =
         this->_snapshot_destination.parent_path() / "archive";
        boost::filesystem::create_directories(archive_path);
        boost::filesystem::path archive_full_path = archive_path / archive_name;
        reactor::background(
          [path, archive_full_path]
          {
            std::vector<boost::filesystem::path> pathes;
            pathes.push_back(path);
            elle::archive::archive(elle::archive::Format::zip_uncompressed,
                                   pathes, archive_full_path);
          });
        this->_add(archive_path, archive_name);
      }
      else
      {
        for (auto it = boost::filesystem::recursive_directory_iterator(path);
             it != end;
             ++it)
        {
          auto parent = path.parent_path();
          boost::filesystem::path relative;
          auto rel = it->path().begin();
          for (auto count = parent.begin(); count != parent.end(); ++count)
            ++rel;
          for (; rel != it->path().end(); ++rel)
            relative /= *rel;
          if (!boost::filesystem::is_directory(*it))
            this->_add(parent, relative);
        }
      }
    }
    else
    {
      ELLE_TRACE("%s: add %s / %s", *this, path.parent_path(), path.filename());
      this->_add(path.parent_path(), path.filename());
    }
  }

  void
  Frete::_add(boost::filesystem::path const& root,
              boost::filesystem::path const& path)
  {
    auto full_path = root / path;

    if (!boost::filesystem::exists(full_path))
      throw elle::Exception(
        elle::sprintf("given path %s doesn't exist", full_path));
    this->_transfer_snapshot->add(root, path);
    // Open the file by making a cache fetch
    if (count() <= _max_count_for_full_cache)
      _fetch_cache(count()-1);
  }

  float
  Frete::progress() const
  {
    if (this->_transfer_snapshot->total_size() == 0)
      return 0.0f;
    ELLE_DUMP("Frete progress(): %s / %s",
              this->_transfer_snapshot->progress(),
              this->_transfer_snapshot->total_size());
    return this->_transfer_snapshot->progress() /
           (float) this->_transfer_snapshot->total_size();
  }

  void
  Frete::save_snapshot() const
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);
    ELLE_DUMP("%s: Saving snapshot %s with progress[0] %s",
                *this,
                this->_snapshot_destination,
                (this->_transfer_snapshot->file_count()?
                  this->_transfer_snapshot->file(0).progress():
                  0))
      ELLE_DUMP("files: %s", this->_transfer_snapshot->files());
    elle::AtomicFile file(this->_snapshot_destination);
    file.write() << [&] (elle::AtomicFile::Write& write)
    {
      elle::serialization::json::SerializerOut output(write.stream(), false);
      this->_transfer_snapshot->serialize(output);
    };
  }

  void
  Frete::remove_snapshot()
  {
    try
    {
      boost::filesystem::remove(this->_snapshot_destination);
    }
    catch (std::exception const&)
    {
      ELLE_ERR("couldn't delete snapshot at %s: %s",
        this->_snapshot_destination, elle::exception_string());
    }
  }

  boost::filesystem::path
  Frete::_local_path(FileID file_id)
  {
    if (this->_transfer_snapshot->mirrored())
    {
      auto file_path = this->_transfer_snapshot->file(file_id).path();
      return (this->_mirror_root / file_path);
    }
    else
    {
      return this->_transfer_snapshot->file(file_id).full_path();
    }
  }

  /*----.
  | Api |
  `----*/

  elle::Version
  Frete::version() const
  {
    return elle::Version(INFINIT_VERSION_MAJOR,
                         INFINIT_VERSION_MINOR,
                         INFINIT_VERSION_SUBMINOR);
  }

  void
  Frete::finish()
  {
    ELLE_TRACE("%s: Finish called, opening barrier", *this);
    this->_transfer_snapshot->file_progress_end(this->count() - 1);
    this->_progress_changed.signal();
    this->_finished.open();
    this->_cache.clear();
  }

  frete::Frete::FileCount
  Frete::count()
  {
    return this->_transfer_snapshot->count();
  }

  frete::Frete::FileSize
  Frete::full_size()
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);
    return this->_transfer_snapshot->total_size();
  }

  frete::Frete::FileSize
  Frete::file_size(FileID file_id)
  {
    return this->_transfer_snapshot->file(file_id).size();
  }

  Frete::FilesInfo
  Frete::files_info()
  {
    Frete::FilesInfo res;
    for (unsigned i = 0; i < this->count(); ++i)
    {
      auto& file = this->_transfer_snapshot->file(i);
      res.push_back(std::make_pair(file.path(), file.size()));
    }
    return res;
  }

  Frete::TransferInfo::TransferInfo(FileCount count,
                                    FileSize full_size,
                                    FilesInfo files_info)
    : _count(count)
    , _full_size(full_size)
    , _files_info(files_info)
  {}

  void
  Frete::TransferInfo::print(std::ostream& stream) const
  {
    stream << "TransferInfo("
           << "count: " << this->_count << ", "
           << "full size: " << this->_full_size << ", "
           << "files infos: ";
    if (this->_files_info.size() < 3)
      stream << this->_files_info;
    else
      stream << this->_files_info.size() << " files";
    stream << ")";
  }

  Frete::TransferInfo
  Frete::transfer_info()
  {
    return Frete::TransferInfo{
      this->count(),
      this->full_size(),
      this->files_info()};
  }

  infinit::cryptography::Code
  Frete::read(FileID f, FileOffset start, FileSize size)
  {
    ELLE_DEBUG("%s: read and encrypt block %s of size %s at offset %s with old key %s",
               *this, f, size, start, this->_impl->old_key());
    return this->_impl->old_key().legacy_encrypt_buffer(this->cleartext_read(f, start, size));
  }

  infinit::cryptography::Code
  Frete::encrypted_read(FileID f, FileOffset start, FileSize size)
  {
    ELLE_DEBUG_SCOPE(
      "%s: read and encrypt block %s of size %s at offset %s with key %s",
      *this, f, size, start, this->_impl->key());

    auto code = this->_impl->key()->legacy_encrypt_buffer(this->cleartext_read(f, start, size));

    ELLE_DUMP("encrypted data: %x", code);
    return code;
  }

  infinit::cryptography::Code
  Frete::encrypted_read_acknowledge(FileID f, FileOffset start, FileSize size,
                                    FileSize acknowledge)
  {
    ELLE_DEBUG_SCOPE(
      "%s: read and encrypt block %s of size %s at offset %s with key %s",
      *this, f, size, start, this->_impl->key());

    auto code = this->_impl->key()->legacy_encrypt_buffer(this->cleartext_read(f, start, size, false));
    auto& snapshot = *this->_transfer_snapshot;
    /* Since we might be pushing both in a bufferer and directly, there
     * are actually two progress positions.
     * We can safely(*) ignore acks behind our current state
    */
    if (acknowledge > snapshot.progress())
      snapshot.progress_increment(acknowledge - snapshot.progress());
    ELLE_DUMP("encrypted data: %x", code);
    return code;
  }

  std::string
  Frete::path(FileID file_id)
  {
    return this->_transfer_snapshot->file(file_id).path();
  }

  elle::Buffer
  Frete::cleartext_read(FileID file_id,
                FileOffset offset,
                FileSize const size,
                bool update_progress)
  {
    ELLE_DEBUG_SCOPE("%s: read %s bytes of file %s at offset %s",
                     *this, size,  file_id, offset);
    auto& snapshot = *this->_transfer_snapshot;
    if (update_progress)
    {
      if (offset != 0)
        snapshot.file_progress_set(file_id, offset);
      else if (file_id != 0)
        snapshot.file_progress_end(file_id - 1);
      this->_progress_changed.signal();
    }
    elle::system::FileHandle& handle = _fetch_cache(file_id);

    elle::Buffer result = handle.read(offset, size);
    if (offset + result.size() > file_size(file_id))
    {
      auto& file = this->_transfer_snapshot->file(file_id);
      std::string message = elle::sprintf(
        "File size inconsistency on %s: %s > %s",
        file.path(), offset + result.size(), file.size());
      ELLE_ERR("%s", message);
      // The sender will reject it and fail the transfer, so throw on this
      // end, the error will be clearer
      throw boost::filesystem::filesystem_error(
        message,
        file.path(),
        boost::system::errc::make_error_code(boost::system::errc::file_too_large)
      );
    }
    return result;
  }

  unsigned int Frete::_max_count_for_full_cache = 20;

  elle::system::FileHandle&
  Frete::_fetch_cache(FileID file_id)
  {
    auto it = _cache.find(file_id);
    if (it == _cache.end())
    {
      auto handle = elle::make_unique<elle::system::FileHandle>(
        this->_local_path(file_id), elle::system::FileHandle::READ);
      _cache[file_id].first = std::move(handle);
    }
    if (count() > _max_count_for_full_cache)
    { // partial cache only, cleanup check
      // kill all entries that got unused more than x times in a row.
      std::vector<int> to_kill;
      for (auto& e: _cache)
      {
        if (e.first == file_id)
        {
          e.second.second=0;
        }
        else
        {
          e.second.second++;
          if (e.second.second > 10)
            to_kill.push_back(e.first);
        }
      }
      for(int i: to_kill)
      {
        _cache.erase(i);
      }
    }
    // we might have moved things that modified the element address. Fetch again.
    return *_cache[file_id].first.get();
  }

  infinit::cryptography::Code
  Frete::key_code() const
  {
    if (!this->_impl->_peer_key)
      throw elle::Exception("Peer key not available, cannot encrypt session key");
    return infinit::cryptography::Code(
      this->_impl->_peer_key->encrypt(*this->_impl->_key));
  }

  void
  Frete::set_progress(FileSize progress)
  {
    // Progress is now handled locally. This information however can be
    // interesting since this is the progress on the client side, excluding any
    // cloud-buffered blocks.
  }

  void
  Frete::print(std::ostream& stream) const
  {
    stream << "Frete";
    // if (this->_transfer_snapshot != nullptr)
    //   stream << " " << *this->_transfer_snapshot;
    // stream << " snapshot location " << this->_snapshot_destination;
  }
}
