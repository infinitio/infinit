#ifndef FRETE_FRETE_HH
# define FRETE_FRETE_HH

# include <ios>
# include <stdint.h>
# include <tuple>
# include <algorithm>

# include <boost/filesystem.hpp>

# include <elle/Buffer.hh>
# include <elle/Version.hh>
# include <elle/container/map.hh>
# include <elle/serialize/BinaryArchive.hh>

# include <protocol/ChanneledStream.hh>
# include <protocol/Serializer.hh>
# include <protocol/RPC.hh>

# include <cryptography/Code.hh>
# include <cryptography/SecretKey.hh>
# include <cryptography/cipher.hh>

# include <frete/fwd.hh>

namespace frete
{
  class Frete:
    public elle::Printable

  {
    friend ::frete::RPCFrete;

  /*------.
  | Types |
  `------*/
  public:
    typedef uint32_t FileID;
    // All integers sent through Trophonius (i.e.: as JSON) are handled as 64bit
    // integers. Best to keep them as such throughout the application.
    typedef uint64_t FileCount;
    typedef uint64_t FileOffset;
    typedef uint64_t FileSize;
    typedef Frete Self;

  /*-------------.
  | Construction |
  `-------------*/
  public:
    Frete(std::string const& password, // Retro compatibility.
          infinit::cryptography::KeyPair const& self_K, /*needed to decrypt session key from snapshot*/
          infinit::cryptography::PublicKey peer_K,
          boost::filesystem::path const& snapshot_destination);
    ~Frete();
  private:
    class Impl;
    ELLE_ATTRIBUTE(std::unique_ptr<Impl>, impl);

  /*----.
  | Run |
  `----*/
  public:
    void
    run();

  /*--------------------.
  | Local configuration |
  `--------------------*/
  public:
    /// Register a file.
    void
    add(boost::filesystem::path const& path);
  private:
    void
    _add(boost::filesystem::path const& root,
         boost::filesystem::path const& path);

  /*----.
  | Api |
  `----*/
  public:
    /// The number of file.
    FileCount
    count();
    /// The total size of files.
    FileSize
    full_size();
    /// The size of a file.
    FileSize
    file_size(FileID f);
    /// The path and size of all files.
    std::vector<std::pair<std::string, FileSize>>
    files_info();
    /// The path of a file.
    std::string
    path(FileID f);
    /// A weakly crypted chunk of a file.
    infinit::cryptography::Code
    read(FileID f, FileOffset start, FileSize size);
    /// A strongly crypted chunk of a file. Aknowlege up to file f position start
    infinit::cryptography::Code
    encrypted_read(FileID f, FileOffset start, FileSize size);
    /// Update the progress.
    void
    set_progress(FileSize progress);
    /// The infinit version.
    elle::Version
    version() const;
    /// The key of the transfer.
    infinit::cryptography::Code const&
    key_code() const;
    /// Signal we're done
    void
    finish();
    /// Request a file chunk and acknowledge overall progress up to
    /// acknowledge_progress starting from the beginning of the whole frete data
    infinit::cryptography::Code
    encrypted_read_acknowledge(FileID f, FileOffset start, FileSize size, FileSize acknowledge_progress);
    /// Whether we're done.
    ELLE_ATTRIBUTE_RX(reactor::Barrier, finished);
  private:
    /// The path of a file on the local filesystem.
    boost::filesystem::path
    _local_path(FileID file_id);
    /// A plain chunk of a file.
    elle::Buffer
    _read(FileID file_id,
          FileOffset offset,
          FileSize const size,
          bool increment_progress = true);

  /*---------.
  | Progress |
  `---------*/
  public:
    float
    progress() const;
    ELLE_ATTRIBUTE_RX(reactor::Signal, progress_changed);

  /*---------.
  | Snapshot |
  `---------*/
  public:
    ELLE_ATTRIBUTE_RX(std::unique_ptr<TransferSnapshot>, transfer_snapshot);
    void
    save_snapshot() const;
    void
    remove_snapshot();
    ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_destination);

  /*----------.
  | Printable |
  `----------*/
  public:
    virtual
    void
    print(std::ostream& stream) const;
  };
}

#endif
