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

  struct Frete::Impl
  {
    // Retrocompatibility with < 0.8.3.
    Impl(std::string const& password):
      _old_key(infinit::cryptography::cipher::Algorithm::aes256, password)
    {}

    virtual
    ~Impl() = default;

    virtual
    infinit::cryptography::SecretKey const&
    key() const = 0;

    ELLE_ATTRIBUTE_R(infinit::cryptography::SecretKey, old_key);
  };

  struct SenderImpl:
    public Frete::Impl
  {
  public:
    SenderImpl(infinit::cryptography::PublicKey const& peer_K,
               std::string const& password):
      Impl(password),
      _key(infinit::cryptography::SecretKey::generate(
             infinit::cryptography::cipher::Algorithm::aes256, 2048)),
      _peer_K(peer_K),
      _encrypted_key(this->_peer_K.encrypt(this->_key))
    {}

    infinit::cryptography::SecretKey const&
    key() const override
    {
      return this->_key;
    }

    infinit::cryptography::Code const&
    encrypted_key() const
    {
      return this->_encrypted_key;
    }

    ELLE_ATTRIBUTE(infinit::cryptography::SecretKey, key);
    ELLE_ATTRIBUTE_R(infinit::cryptography::PublicKey, peer_K);
    ELLE_ATTRIBUTE(infinit::cryptography::Code, encrypted_key);
  };

  struct RecipientImpl:
    public Frete::Impl
  {
    RecipientImpl(infinit::cryptography::PrivateKey k,
                  std::string const& password):
      Impl(password),
      _k(k),
      _encrypted_key(nullptr),
      _key(nullptr)
    {}

    infinit::cryptography::SecretKey const&
    key() const override
    {
      ELLE_ASSERT(this->_encrypted_key != nullptr);
      if (!this->_key)
        this->_key.reset(
          new infinit::cryptography::SecretKey(
            this->_k.decrypt<infinit::cryptography::SecretKey>(*this->_encrypted_key)));
      return *this->_key;
    }

    void
    encrypted_key(infinit::cryptography::Code const& code)
    {
      this->_encrypted_key.reset(new infinit::cryptography::Code(code));
    }

    bool
    has_encrypted_key()
    {
      return this->_encrypted_key != nullptr;
    }

    ELLE_ATTRIBUTE(infinit::cryptography::PrivateKey, k);
    ELLE_ATTRIBUTE(std::unique_ptr<infinit::cryptography::Code>, encrypted_key);
    ELLE_ATTRIBUTE_P(std::unique_ptr<infinit::cryptography::SecretKey>, key, mutable);
  };

  Frete::Frete(infinit::protocol::ChanneledStream& channels,
               boost::filesystem::path const& snapshot_destination,
               bool):
    _impl(nullptr),
    _rpc(channels),
    _rpc_count("count", this->_rpc),
    _rpc_full_size("full_size", this->_rpc),
    _rpc_file_size("size", this->_rpc),
    _rpc_path("path", this->_rpc),
    _rpc_read("read", this->_rpc),
    _rpc_set_progress("progress", this->_rpc),
    _rpc_version("version", this->_rpc),
    _rpc_key_code("key_code", this->_rpc),
    _rpc_encrypted_read("encrypted_read", this->_rpc),
    _rpc_finish("finish", this->_rpc),
    _progress_changed("progress changed signal"),
    _snapshot_destination(snapshot_destination),
    _transfer_snapshot{}
  {
    this->_rpc_count = std::bind(&Self::_count,
                                this);
    this->_rpc_full_size = std::bind(&Self::_full_size,
                                this);
    this->_rpc_file_size = std::bind(&Self::_file_size,
                                     this,
                                     std::placeholders::_1);
    this->_rpc_path = std::bind(&Self::_path,
                                this,
                                std::placeholders::_1);
    this->_rpc_read = std::bind(&Self::_read,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);
    this->_rpc_set_progress = std::bind(&Self::_set_progress,
                                        this,
                                        std::placeholders::_1);

    this->_rpc_version = std::bind(&Self::_version, this);
    this->_rpc_key_code = std::bind(&Self::_key_code,
                                    this);
    this->_rpc_encrypted_read = std::bind(&Self::_encrypted_read,
                                          this,
                                          std::placeholders::_1,
                                          std::placeholders::_2,
                                          std::placeholders::_3);
    this->_rpc_finish = std::bind(std::bind(&Self::_finish, this));

    ELLE_DEBUG("%s: looking for snapshot at %s",
               *this, this->_snapshot_destination);
    if (boost::filesystem::exists(this->_snapshot_destination))
    {
      ELLE_LOG("%s: snapshot exist at %s", *this, this->_snapshot_destination);
      try
      {
        elle::SafeFinally delete_snapshot{
          [&]
          {
            try
            {
              boost::filesystem::remove(this->_snapshot_destination);
            }
            catch (std::exception const&)
            {
              ELLE_ERR("%s: couldn't delete snapshot at %s: %s", *this,
                       this->_snapshot_destination, elle::exception_string());
            }
          }};

        this->_transfer_snapshot.reset(
          new TransferSnapshot(
            elle::serialize::from_file(this->_snapshot_destination.string())));
      }
      catch (std::exception const&) //XXX: Choose the right exception here.
      {
        ELLE_ERR("%s: snap shot was invalid: %s", *this, elle::exception_string());
      }
    }
  }

  // Frete::Frete(infinit::protocol::ChanneledStream& channels,
  //              std::string const& password, // Retro compatibility.
  //              boost::filesystem::path const& snapshot_destination):
  //   Frete(channels, snapshot_destination, false)
  // {
  //   ELLE_ASSERT(this->_impl == nullptr);
  //   this->_impl.reset(new ReceiveImpl(password));
  // }

  // Sender.
  Frete::Frete(infinit::protocol::ChanneledStream& channels,
               std::string const& password, // Retro compatibility.
               infinit::cryptography::PublicKey peer_K,
               boost::filesystem::path const& snapshot_destination):
    Frete(channels, snapshot_destination, false)
  {
    ELLE_ASSERT(this->_impl == nullptr);
    this->_impl.reset(new SenderImpl(peer_K, password));
  }

  Frete::Frete(infinit::protocol::ChanneledStream& channels,
               std::string const& password, // Retro compatibility.
               infinit::cryptography::PrivateKey self_k,
               boost::filesystem::path const& snapshot_destination):
    Frete(channels, snapshot_destination, false)
  {
    ELLE_ASSERT(this->_impl == nullptr);
    this->_impl.reset(new RecipientImpl(self_k, password));
  }

  Frete::~Frete()
  {}

  boost::filesystem::path
  Frete::trim(boost::filesystem::path const& item,
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

    // XXX: Frete is used as sender or recipient...
    // Add is dedicated to sender.
    if (this->_transfer_snapshot == nullptr)
      this->_transfer_snapshot.reset(new TransferSnapshot());

    auto& snapshot = *this->_transfer_snapshot;

    ELLE_ASSERT(snapshot.sender());

    snapshot.add(root, path);

    ELLE_DEBUG("%s: updated snapshot: %s", *this, snapshot);
  }

  boost::filesystem::path
  Frete::eligible_name(boost::filesystem::path const path,
                       std::string const& name_policy)
  {
    if (!boost::filesystem::exists(path))
      return path;

    auto _path = path.filename();
    // Remove the extensions, add the name_policy and set the extension.
    std::string extensions;
    for (; !_path.extension().empty(); _path = _path.stem())
      extensions = _path.extension().string() + extensions;
    _path = path.parent_path() / _path;
    _path += name_policy;
    _path += extensions;

    // Ugly.
    for (size_t i = 2; i < std::numeric_limits<size_t>::max(); ++i)
    {
      if (!boost::filesystem::exists(elle::sprintf(_path.string().c_str(), i)))
      {
        return elle::sprintf(_path.string().c_str(), i);
      }
    }

    throw elle::Exception(
      elle::sprintf("unable to find a suitable name that matches %s", _path));
  }

  void
  Frete::get(boost::filesystem::path const& output_path,
             bool strong_encryption,
             std::string const& name_policy)
  {
    auto peer_version = this->version();
    if (peer_version < elle::Version(0, 8, 3))
    {
      // XXX: Create better exception.
      if (strong_encryption)
        ELLE_WARN("peer version doesn't support strong encryption");
      strong_encryption = false;
    }

    FileCount count = this->_rpc_count();

    // total_size can be 0 if all files are empty.
    auto total_size = this->_rpc_full_size();

    if (this->_transfer_snapshot != nullptr)
    {
      if ((this->_transfer_snapshot->total_size() != total_size) ||
          (this->_transfer_snapshot->count() != count))
      {
        ELLE_ERR("%s: snapshot data (%s) are invalid",
                 this, *this->_transfer_snapshot);
        throw elle::Exception("invalid transfer data");
      }
    }
    else
    {
      this->_transfer_snapshot.reset(new TransferSnapshot(count, total_size));
    }

    static std::streamsize const chunk_size = 1 << 18;

    auto& snapshot = *this->_transfer_snapshot;

    ELLE_DEBUG("%s: transfer snapshot: %s", *this, snapshot);

    FileCount last_index = snapshot.transfers().size();
    if (last_index > 0)
      --last_index;

    // If files are present in the snapshot, take the last one.
    for (FileCount index = last_index; index < count; ++index)
    {
      ELLE_DEBUG("%s: index %s", *this, index);

      boost::filesystem::path fullpath;
      // XXX: Merge file_size & rpc_path.
      auto file_size = this->_rpc_file_size(index);

      if (snapshot.transfers().find(index) != snapshot.transfers().end())
      {
        auto const& transfer = snapshot.transfers().at(index);
        fullpath = snapshot.transfers().at(index).full_path();

        if (file_size != transfer.file_size())
        {
          ELLE_ERR("%s: transfer data (%s) at index %s are invalid.",
                   *this, transfer, index);

          throw elle::Exception("invalid transfer data");
        }
      }
      else
      {
        auto relativ_path = boost::filesystem::path{this->_rpc_path(index)};
        fullpath = Frete::eligible_name(output_path / relativ_path,
                                        name_policy);
        relativ_path = Frete::trim(fullpath, output_path);

        snapshot.transfers().emplace(
          std::piecewise_construct,
          std::make_tuple(index),
          std::forward_as_tuple(index, output_path, relativ_path, file_size));
      }

      ELLE_ASSERT(snapshot.transfers().find(index) != snapshot.transfers().end());

      auto& tr = snapshot.transfers().at(index);

      ELLE_DEBUG("%s: index (%s) - path %s - size %s",
                 *this, index, fullpath, file_size);

      // Create subdir.
      boost::filesystem::create_directories(fullpath.parent_path());
      boost::filesystem::ofstream output{fullpath,
                                         std::ios::app | std::ios::binary};

      if (tr.complete())
      {
        ELLE_DEBUG("%s: transfer was marked as complete", *this);
        continue;
      }

      while (true)
      {
        if (!output.good())
          throw elle::Exception("output is invalid");

        // Get the buffer from the rpc.
        elle::Buffer buffer{strong_encryption ?
            this->encrypted_read(index, tr.progress(), chunk_size) :
            this->read(index, tr.progress(), chunk_size)};

        ELLE_ASSERT_LT(index, snapshot.transfers().size());
        ELLE_DUMP("%s: %s (size: %s)", index, fullpath, boost::filesystem::file_size(fullpath));

        {
          boost::system::error_code ec;
          FileSize size = boost::filesystem::file_size(fullpath, ec);

          if (ec)
          {
            ELLE_ERR("destination file deleted");
            throw elle::Exception("destination file deleted");
          }

          if (size != snapshot.transfers()[index].progress())
          {
            uintmax_t current_size;
            try
            {
              current_size = boost::filesystem::file_size(fullpath);
            }
            catch (boost::filesystem::filesystem_error const& e)
            {
              ELLE_ERR("%s: expected size and actual size differ, "
                       "unable to determine actual size: %s", *this, e);
              throw elle::Exception("destination file corrupted");
            }
            ELLE_ERR(
              "%s: expected file size %s and actual file size %s are different",
              *this,
              snapshot.transfers()[index].progress(),
              current_size);
            throw elle::Exception("destination file corrupted");
          }
        }

        ELLE_DUMP("content: %s (%sB)", buffer, buffer.size());

        // Write the file.
        output.write((char const*) buffer.contents(), buffer.size());
        output.flush();

        if (!output.good())
          elle::Exception("writing left the stream in a bad state");

        {
          snapshot.increment_progress(index, buffer.size());
          elle::serialize::to_file(this->_snapshot_destination.string()) << *this->_transfer_snapshot;
          this->_rpc_set_progress(this->_transfer_snapshot->progress());
          this->_progress_changed.signal();
          // this->_rpc_set_progress(this->_transfer_snapshot->progress());
        }

        ELLE_DEBUG("%s: %s (size: %s)",
                   index, fullpath, boost::filesystem::file_size(fullpath));
        ELLE_ASSERT_EQ(boost::filesystem::file_size(fullpath),
                       snapshot.transfers()[index].progress());

        if (buffer.size() < unsigned(chunk_size))
        {
          output.close();
          ELLE_TRACE("finished %s: %s", index, snapshot);
          break;
        }
      }
    }
    this->_finished.open();
    this->finish();
    try
    {
      boost::filesystem::remove(this->_snapshot_destination);
    }
    catch (std::exception const&)
    {
      ELLE_ERR("%s: couldn't delete snapshot at %s: %s", *this,
               this->_snapshot_destination, elle::exception_string());
    }
  }

  float
  Frete::progress() const
  {
    if (this->_transfer_snapshot == nullptr)
      return 0.0f;

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

  /*-------------.
  | Remote calls |
  `-------------*/

  frete::Frete::FileCount
  Frete::count()
  {
    return this->_rpc_count();
  }

  frete::Frete::FileSize
  Frete::full_size()
  {
    return this->_rpc_full_size();
  }

  frete::Frete::FileSize
  Frete::file_size(FileID f)
  {
    return this->_rpc_file_size(f);
  }

  std::string
  Frete::path(FileID f)
  {
    return this->_rpc_path(f);
  }

  elle::Buffer
  Frete::read(FileID f, FileOffset start, FileSize size)
  {
    return this->_impl->old_key().decrypt<elle::Buffer>(
      this->_rpc_read(f, start, size));
  }

  elle::Buffer
  Frete::encrypted_read(FileID f, FileOffset start, FileSize size)
  {
    return this->key().decrypt<elle::Buffer>(
      this->_rpc_encrypted_read(f, start, size));
  }

  void
  Frete::finish()
  {
    this->_rpc_finish();
  }

  elle::Version
  Frete::version()
  {
    try
    {
      return this->_rpc_version();
    }
    catch (infinit::protocol::RPCError&)
    {
      // Before version 0.8.2, the version RPC did not exist. 0.7 is the oldest
      // public version.
      return elle::Version(0, 7, 0);
    }
  }

  /*-----.
  | RPCs |
  `-----*/

  boost::filesystem::path
  Frete::_local_path(FileID file_id)
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);

    ELLE_ASSERT(this->_transfer_snapshot->transfers().find(file_id) !=
                this->_transfer_snapshot->transfers().end());

    return this->_transfer_snapshot->transfers().at(file_id).full_path();
  }

  frete::Frete::FileCount
  Frete::_count()
  {
    ELLE_DEBUG("%s: get file count", *this);
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);

    ELLE_DEBUG("%s: %s file(s)",
               *this, this->_transfer_snapshot->transfers().size());
    return this->_transfer_snapshot->transfers().size();
  }

  frete::Frete::FileSize
  Frete::_full_size()
  {
    ELLE_ASSERT(this->_transfer_snapshot != nullptr);

    return this->_transfer_snapshot->total_size();
  }

  frete::Frete::FileSize
  Frete::_file_size(FileID file_id)
  {
    ELLE_ASSERT_LT(file_id, this->_count());

    ELLE_ASSERT(this->_transfer_snapshot->transfers().find(file_id) !=
                this->_transfer_snapshot->transfers().end());

    return this->_transfer_snapshot->transfers().at(file_id).file_size();
  }

  std::string
  Frete::_path(FileID file_id)
  {
    ELLE_ASSERT_LT(file_id, this->_count());

    ELLE_ASSERT(this->_transfer_snapshot->transfers().find(file_id) !=
                this->_transfer_snapshot->transfers().end());

    return this->_transfer_snapshot->transfers().at(file_id).path();
  }

  elle::Buffer
  Frete::__read(FileID file_id,
                FileOffset offset,
                FileSize const size)
  {
    ELLE_TRACE_FUNCTION(file_id, offset, size);
    ELLE_ASSERT_LT(file_id, this->_count());

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

    file.seekg(offset, std::ios_base::cur);

    if (!file.good())
      throw elle::Exception(
        elle::sprintf("unable to seek to pos %s", offset));

    elle::Buffer buffer(size);

    file.read(reinterpret_cast<char*>(buffer.mutable_contents()), size);
    buffer.size(file.gcount());

    if (!file.eof() && file.fail() || file.bad())
      throw elle::Exception("unable to read");

    return buffer;
  }

  infinit::cryptography::Code
  Frete::_read(FileID file_id,
               FileOffset offset,
               FileSize const size)
  {
    return this->_impl->old_key().encrypt(this->__read(file_id, offset, size));
  }

  infinit::cryptography::Code
  Frete::_encrypted_read(FileID file_id,
                         FileOffset offset,
                         FileSize const size)
  {
    return this->_impl->key().encrypt(this->__read(file_id, offset, size));
  }

  void
  Frete::_finish()
  {
    auto& snapshot = *this->_transfer_snapshot;
    snapshot.end_progress(this->_count() - 1);
    this->_progress_changed.signal();
    this->_finished.open();
  }

  infinit::cryptography::Code const&
  Frete::_key_code() const
  {
    ELLE_ASSERT(dynamic_cast<SenderImpl*>(this->_impl.get()));
    return static_cast<SenderImpl*>(this->_impl.get())->encrypted_key();
  }

  infinit::cryptography::SecretKey const&
  Frete::key()
  {
    ELLE_ASSERT(dynamic_cast<RecipientImpl*>(this->_impl.get()));
    if (!static_cast<RecipientImpl*>(this->_impl.get())->has_encrypted_key())
    {
      ELLE_TRACE("%s: fetch key code from sender", *this);
      static_cast<RecipientImpl*>(this->_impl.get())->encrypted_key(
        this->_rpc_key_code());
    }

    return static_cast<RecipientImpl*>(this->_impl.get())->key();
  }

  void
  Frete::_set_progress(FileSize progress)
  {
    // Progress is now handled locally. This information however can be
    // interesting since this is the progress on the client side, excluding any
    // cloud-buffered blocks.
  }

  elle::Version
  Frete::_version() const
  {
    return elle::Version(INFINIT_VERSION_MAJOR,
                         INFINIT_VERSION_MINOR,
                         INFINIT_VERSION_SUBMINOR);
  }

  void
  Frete::run()
  {
    ELLE_TRACE("%s: run", *this);
    this->_rpc.run();
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
