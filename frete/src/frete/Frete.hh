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
          infinit::cryptography::PublicKey peer_K,
          boost::filesystem::path const& snapshot_destination);

  public:
    ~Frete();

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
    void
    add(boost::filesystem::path const& path);
    void
    add(boost::filesystem::path const& root,
        boost::filesystem::path const& path);

    /*----.
    | Api |
    `----*/
  public:
    /// Return the number of file.
    FileCount
    count();
    /// Return the total size of files.
    FileSize
    full_size();
    /// Return the size of a file.
    FileSize
    file_size(FileID f);
    /// Return the path and size of all files.
    std::vector<std::pair<std::string, FileSize>>
    files_info();
    /// Return the path of a file.
    std::string
    path(FileID f);
    /// Return a weakly crypted chunck of a file.
    infinit::cryptography::Code
    read(FileID f, FileOffset start, FileSize size);
    /// Return a strongly crypted chunck of a file.
    infinit::cryptography::Code
    encrypted_read(FileID f, FileOffset start, FileSize size);
    /// Update the progress.
    void
    set_progress(FileSize progress);
    /// Return the version.
    elle::Version
    version() const;
    /// Get the key of the transfer.
    infinit::cryptography::Code const&
    key_code() const;
    /// Signal we're done
    void
    finish();

    ELLE_ATTRIBUTE_RX(reactor::Barrier, finished);

  private:
    boost::filesystem::path
    _local_path(FileID file_id);
    elle::Buffer
    _read(FileID file_id,
          FileOffset offset,
          FileSize const size);

  private:
    void
    _save_snapshot() const;

    ELLE_ATTRIBUTE_RX(reactor::Signal, progress_changed);
  public:
    float
    progress() const;

  private:
    ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_destination);
    ELLE_ATTRIBUTE_R(std::unique_ptr<TransferSnapshot>, transfer_snapshot);

  /*----------.
  | Printable |
  `----------*/
  public:
    virtual
    void
    print(std::ostream& stream) const;

  /*--------.
  | Helpers |
  `--------*/
  private:
    void
    _check_file_id(FileID id);
  };
}

#endif
