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
    Impl(infinit::cryptography::PublicKey const& peer_key,
         std::string const& password):
      _old_key(infinit::cryptography::cipher::Algorithm::aes256, password),
      _key(infinit::cryptography::SecretKey::generate(
             infinit::cryptography::cipher::Algorithm::aes256, 2048)),
      _peer_key(peer_key),
      _encrypted_key(this->_peer_key.encrypt(this->_key))
    {
      ELLE_DEBUG("frete impl with peer K %s", peer_key);
    }

    ELLE_ATTRIBUTE_R(infinit::cryptography::SecretKey, old_key);
    ELLE_ATTRIBUTE_R(infinit::cryptography::SecretKey, key);
    ELLE_ATTRIBUTE_R(infinit::cryptography::PublicKey, peer_key);
    ELLE_ATTRIBUTE_R(infinit::cryptography::Code, encrypted_key);
  };

  Frete::Frete(std::string const& password,
               infinit::cryptography::PublicKey peer_key,
               boost::filesystem::path const& snapshot_destination):
    _impl(new Impl(peer_key, password)),
    _progress_changed("progress changed signal"),
    _snapshot_destination(snapshot_destination),
    _transfer_snapshot()
  {
    if (boost::filesystem::exists(this->_snapshot_destination))
    {
      ELLE_TRACE_SCOPE("%s: load snapshot %s",
                       *this, this->_snapshot_destination);
      try
      {
        this->_transfer_snapshot.reset(
          new TransferSnapshot(
            elle::serialize::from_file(this->_snapshot_destination.string())));
      }
      catch (std::exception const&) //XXX: Choose the right exception here.
      {
        ELLE_ERR("%s: snapshot is invalid: %s",
                 *this, elle::exception_string());
        boost::filesystem::remove(this->_snapshot_destination);
      }
    }
    if (this->_transfer_snapshot == nullptr)
      this->_transfer_snapshot.reset(new TransferSnapshot());
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
          this->add(parent, relative);
      }
    }
    else
    {
      ELLE_TRACE("%s: add %s / %s", *this, path.parent_path(), path.filename());
      this->add(path.parent_path(), path.filename());
    }
  }

  void
  Frete::add(boost::filesystem::path const& root,
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
  Frete::_save_snapshot() const
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);
    elle::serialize::to_file(this->_snapshot_destination.string()) <<
      *this->_transfer_snapshot;
  }

  boost::filesystem::path
  Frete::_local_path(FileID file_id)
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);
    ELLE_ASSERT(this->_transfer_snapshot->transfers().find(file_id) !=
                this->_transfer_snapshot->transfers().end());
    return this->_transfer_snapshot->transfers().at(file_id).full_path();
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
    this->_transfer_snapshot->end_progress(this->count() - 1);
    this->_progress_changed.signal();
    this->_finished.open();
  }

  frete::Frete::FileCount
  Frete::count()
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);
    ELLE_DEBUG("%s: %s file(s)",
               *this, this->_transfer_snapshot->transfers().size());
    return this->_transfer_snapshot->transfers().size();
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
    ELLE_ASSERT_LT(file_id, this->count());

    ELLE_ASSERT(this->_transfer_snapshot->transfers().find(file_id) !=
                this->_transfer_snapshot->transfers().end());

    return this->_transfer_snapshot->transfers().at(file_id).file_size();
  }

  std::vector<std::pair<std::string, Frete::FileSize>>
  Frete::files_info()
  {
    std::vector<std::pair<std::string, FileSize>> res;
    for (unsigned i = 0; i < this->count(); ++i)
    {
      auto& transfer = this->_transfer_snapshot->transfers().at(i);
      res.push_back(std::make_pair(transfer.path(), transfer.file_size()));
    }
    return res;
  }

  infinit::cryptography::Code
  Frete::read(FileID f, FileOffset start, FileSize size)
  {
    ELLE_DEBUG("%s: read and encrypt block %s of size %s at offset %s with old key %s",
               *this, f, size, start, this->_impl->old_key());
    return this->_impl->old_key().encrypt(this->_read(f, start, size));
  }

  infinit::cryptography::Code
  Frete::encrypted_read(FileID f, FileOffset start, FileSize size)
  {
    ELLE_TRACE_SCOPE(
      "%s: read and encrypt block %s of size %s at offset %s with key %s",
      *this, f, size, start, this->_impl->key());

    auto code = this->_impl->key().encrypt(this->_read(f, start, size));

    ELLE_DUMP("encrypted data: %s with buffer %x", code, code.buffer());
    return code;
  }

  std::string
  Frete::path(FileID file_id)
  {
    this->_check_file_id(file_id);
    ELLE_ASSERT(this->_transfer_snapshot->transfers().find(file_id) !=
                this->_transfer_snapshot->transfers().end());

    return this->_transfer_snapshot->transfers().at(file_id).path();
  }

  elle::Buffer
  Frete::_read(FileID file_id,
                FileOffset offset,
                FileSize const size)
  {
    ELLE_DEBUG_SCOPE("%s: read %s bytes of file %s at offset %s",
                     *this, size,  file_id, offset);
    this->_check_file_id(file_id);

    auto& snapshot = *this->_transfer_snapshot;
    if (offset != 0)
      snapshot.set_progress(file_id, offset);
    else if (file_id != 0)
      snapshot.end_progress(file_id - 1);
    this->_progress_changed.signal();

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
      throw elle::Exception("unable to read");

    ELLE_DUMP("buffer read: %x", buffer);
    return buffer;
  }

  infinit::cryptography::Code const&
  Frete::key_code() const
  {
    return this->_impl->encrypted_key();
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

  void
  Frete::_check_file_id(FileID id)
  {
    if (id >= this->count())
      throw elle::Exception(elle::sprintf("file id out of range: %s", id));
  }
}
