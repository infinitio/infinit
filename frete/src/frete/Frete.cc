#include <functional>
#include <ios>
#include <limits>
#include <algorithm>

#include <boost/filesystem/fstream.hpp>

#include <elle/finally.hh>
#include <elle/serialize/construct.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>

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
    Impl(std::string const& password):
      _old_key(infinit::cryptography::cipher::Algorithm::aes256, password),
      _key(new infinit::cryptography::SecretKey(
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
               boost::filesystem::path const& snapshot_destination):
    _impl(new Impl(password)),
    _progress_changed("progress changed signal"),
    _transfer_snapshot(),
    _snapshot_destination(snapshot_destination)
  {
    ELLE_TRACE_SCOPE("%s: load snapshot %s",
                     *this, this->_snapshot_destination);
    try
    {
      this->_transfer_snapshot.reset(
        new TransferSnapshot(
          elle::serialize::from_file(this->_snapshot_destination.string())));
      ELLE_DUMP("%s: Loaded snapshot %s with progress[0] %s",
                *this,
                this->_snapshot_destination,
                (this->_transfer_snapshot->file_count()?
                  this->_transfer_snapshot->file(0).progress():
                  0));
      // reload key from snapshot
      auto& k = *this->_transfer_snapshot->key_code();
      _impl->_key.reset(
        new infinit::cryptography::SecretKey(
          self_key.k().decrypt<infinit::cryptography::SecretKey>(k)));
    }
    catch (boost::filesystem::filesystem_error const&)
    {
      ELLE_TRACE("%s: unable to read snapshot file: %s",
                 *this, elle::exception_string());
    }
    catch (std::exception const&) //XXX: Choose the right exception here.
    {
      ELLE_WARN("%s: snapshot is invalid: %s",
                *this, elle::exception_string());
    }
    if (this->_transfer_snapshot == nullptr)
    {
      this->_transfer_snapshot.reset(new TransferSnapshot());
      // Write encrypted session key to the snapshot
      this->_transfer_snapshot->set_key_code(self_key.K().encrypt(*_impl->_key));
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
      boost::filesystem::recursive_directory_iterator end;
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
  }

  float
  Frete::progress() const
  {
    if (this->_transfer_snapshot->total_size() == 0)
      return 0.0f;
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
                  0));
    elle::serialize::to_file(this->_snapshot_destination.string()) <<
      *this->_transfer_snapshot;
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
    return this->_transfer_snapshot->file(file_id).full_path();
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

  std::vector<std::pair<std::string, Frete::FileSize>>
  Frete::files_info()
  {
    std::vector<std::pair<std::string, FileSize>> res;
    for (unsigned i = 0; i < this->count(); ++i)
    {
      auto& file = this->_transfer_snapshot->file(i);
      res.push_back(std::make_pair(file.path(), file.size()));
    }
    return res;
  }

  infinit::cryptography::Code
  Frete::read(FileID f, FileOffset start, FileSize size)
  {
    ELLE_DEBUG("%s: read and encrypt block %s of size %s at offset %s with old key %s",
               *this, f, size, start, this->_impl->old_key());
    return this->_impl->old_key().encrypt(this->cleartext_read(f, start, size));
  }

  infinit::cryptography::Code
  Frete::encrypted_read(FileID f, FileOffset start, FileSize size)
  {
    ELLE_DEBUG_SCOPE(
      "%s: read and encrypt block %s of size %s at offset %s with key %s",
      *this, f, size, start, this->_impl->key());

    auto code = this->_impl->key()->encrypt(this->cleartext_read(f, start, size));

    ELLE_DUMP("encrypted data: %s with buffer %x", code, code.buffer());
    return code;
  }

  infinit::cryptography::Code
  Frete::encrypted_read_acknowledge(FileID f, FileOffset start, FileSize size,
                                    FileSize acknowledge)
  {
    ELLE_DEBUG_SCOPE(
      "%s: read and encrypt block %s of size %s at offset %s with key %s",
      *this, f, size, start, this->_impl->key());

    auto code = this->_impl->key()->encrypt(this->cleartext_read(f, start, size, false));
    auto& snapshot = *this->_transfer_snapshot;
    /* Since we might be pushing both in a bufferer and directly, there
     * are actually two progress positions.
     * We can safely(*) ignore acks behind our current state
    */
    if (acknowledge > snapshot.progress())
      snapshot.progress_increment(acknowledge - snapshot.progress());
    ELLE_DUMP("encrypted data: %s with buffer %x", code, code.buffer());
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
    auto path = this->_local_path(file_id);
    boost::filesystem::ifstream file{path, std::ios::binary};
    static const FileOffset MAX_offset{
      std::numeric_limits<std::streamsize>::max()};
    static const size_t MAX_buffer{elle::Buffer::max_size};

    if (size > MAX_buffer)
      throw elle::Exception(
        elle::sprintf("buffer that big (%s) can't be addressed", size));

    if (!file.good())
      throw elle::Exception("file is broken");

    // If the offset is greater than the machine maximum streamsize, seekg n
    // times to reach the right offset.
    while (offset > MAX_offset)
    {
      file.seekg(MAX_offset, std::ios_base::cur);

      if (!file.good())
        throw elle::Exception(
          elle::sprintf("unable to increment offset by %s", MAX_offset));

      offset -= MAX_offset;
    }

    ELLE_DEBUG("seek to offset %s", *this);
    file.seekg(offset, std::ios_base::cur);

    if (!file.good())
      throw elle::Exception(
        elle::sprintf("unable to seek to pos %s", offset));

    elle::Buffer buffer(size);

    ELLE_DEBUG("read  file");
    file.read(reinterpret_cast<char*>(buffer.mutable_contents()), size);
    buffer.size(file.gcount());

    ELLE_DEBUG("buffer resized to %s bytes", buffer.size());

    if (!file.eof() && file.fail() || file.bad())
      throw elle::Exception("unable to reathd");

    ELLE_DUMP("buffer read: %x", buffer);
    return buffer;
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
