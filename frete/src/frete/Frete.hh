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
    // Recipient.
    Frete(infinit::protocol::ChanneledStream& channels,
          std::string const& password, // Retro compatibility.
          infinit::cryptography::PrivateKey self_k,
          boost::filesystem::path const& snapshot_destination);

    // Sender.
    Frete(infinit::protocol::ChanneledStream& channels,
          std::string const& password, // Retro compatibility.
          infinit::cryptography::PublicKey peer_K,
          boost::filesystem::path const& snapshot_destination);

  private:
    Frete(infinit::protocol::ChanneledStream& channels,
          boost::filesystem::path const& snapshot_destination,
          bool);

  public:
    ~Frete();

    struct Impl;
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
    void
    add(boost::filesystem::path const& path);
    void
    add(boost::filesystem::path const& root,
        boost::filesystem::path const& path);
    void
    get(boost::filesystem::path const& output,
        bool strong_encryption = true,
        std::string const& name_policy = " (%s)");

    /*-------------.
    | Remote calls |
    `-------------*/
  public:
    /// The number of file in the remote filesystem.
    FileCount
    count();
    /// The total size of a remote files.
    FileSize
    full_size();
    /// The size of a remote file.
    FileSize
    file_size(FileID f);
    /// The path of the \a f file.
    std::string
    path(FileID f);
    /// A chunk of a remote file.
    elle::Buffer
    read(FileID f, FileOffset start, FileSize size);
    elle::Buffer
    encrypted_read(FileID f, FileOffset start, FileSize size);
    /// Notify the sender of the progress of the transaction.
    void
    set_progress(FileSize progress);
    /// The remote Infinit version.
    elle::Version
    version();
    /// Get the key of the transfer.
    infinit::cryptography::SecretKey const&
    key();
    /// Signal we're done
    void
    finish();

    ELLE_ATTRIBUTE_RX(reactor::Barrier, finished);

  /*-----.
  | RPCs |
  `-----*/
  private:
    boost::filesystem::path
    _local_path(FileID file_id);
    FileCount
    _count();
    FileSize
    _full_size();
    FileSize
    _file_size(FileID f);
    std::string
    _path(FileID f);
    elle::Buffer
    __read(FileID file_id,
           FileOffset offset,
           FileSize const size);
    infinit::cryptography::Code
    _read(FileID f, FileOffset start, FileSize size);
    infinit::cryptography::Code
    _encrypted_read(FileID f, FileOffset start, FileSize size);
    void
    _finish();
    void
    _set_progress(FileSize progress);
    elle::Version
    _version() const;
    infinit::cryptography::Code const&
    _key_code() const;

    // Sender.
    typedef std::pair<boost::filesystem::path, boost::filesystem::path> Path;
    typedef std::vector<Path> Paths;
    ELLE_ATTRIBUTE(Paths, paths);

    typedef infinit::protocol::RPC<
      elle::serialize::InputBinaryArchive,
      elle::serialize::OutputBinaryArchive> RPC;
    RPC _rpc;
    RPC::RemoteProcedure<FileCount> _rpc_count;
    RPC::RemoteProcedure<FileSize> _rpc_full_size;
    RPC::RemoteProcedure<FileSize, FileID> _rpc_file_size;
    RPC::RemoteProcedure<std::string,
                         FileID> _rpc_path;
    RPC::RemoteProcedure<infinit::cryptography::Code,
                         FileID,
                         FileOffset,
                         FileSize> _rpc_read;
    RPC::RemoteProcedure<void,
                         FileSize> _rpc_set_progress;
    RPC::RemoteProcedure<elle::Version> _rpc_version;
    RPC::RemoteProcedure<infinit::cryptography::Code> _rpc_key_code;
    RPC::RemoteProcedure<infinit::cryptography::Code,
                         FileID,
                         FileOffset,
                         FileSize> _rpc_encrypted_read;
    RPC::RemoteProcedure<void> _rpc_finish;

  private:
    void
    _save_snapshot() const;

    ELLE_ATTRIBUTE_RX(reactor::Signal, progress_changed);
  public:
    float
    progress() const;

  private:
    ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_destination);
    ELLE_ATTRIBUTE(std::unique_ptr<TransferSnapshot>, transfer_snapshot);

    /*----------.
    | Printable |
    `----------*/
  public:
    virtual
    void
    print(std::ostream& stream) const;

    /*--------------.
    | Static Method |
    `--------------*/
    // Exposed for debugging purposes.
    static
    boost::filesystem::path
    eligible_name(boost::filesystem::path const path,
                  std::string const& name_policy);

    static
    boost::filesystem::path
    trim(boost::filesystem::path const& item,
         boost::filesystem::path const& root);
  };
}

#endif
