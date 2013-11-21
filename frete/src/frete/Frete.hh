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
    typedef uint64_t Offset;
    typedef uint64_t Size;
    typedef Frete Self;

  /*-------------.
  | Construction |
  `-------------*/
  public:
    Frete(infinit::protocol::ChanneledStream& channels,
          std::string const& passphrase,
          boost::filesystem::path const& snapshot_destination);
    ~Frete();

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
        std::string const& name_policy = " (%s)");

    /*-------------.
    | Remote calls |
    `-------------*/
  public:
    /// The number of file in the remote filesystem.
    uint64_t
    count();
    /// The total size of a remote files.
    uint64_t
    full_size();
    /// The size of a remote file.
    uint64_t
    file_size(FileID f);
    /// The path of the \a f file.
    std::string
    path(FileID f);
    /// A chunk of a remote file.
    elle::Buffer
    read(FileID f, Offset start, Size size);
    /// Notify the sender of the progress of the transaction.
    void
    set_progress(uint64_t progress);

    ELLE_ATTRIBUTE_R(reactor::Barrier, finished);

  /*-----.
  | RPCs |
  `-----*/
  private:
    boost::filesystem::path
    _local_path(FileID file_id);
    uint64_t
    _count();
    uint64_t
    _full_size();
    uint64_t
    _file_size(FileID f);
    std::string
    _path(FileID f);
    infinit::cryptography::Code
    _read(FileID f, Offset start, Size size);
    void
    _set_progress(uint64_t progress);
    elle::Version
    _version() const;

    // Sender.
    typedef std::pair<boost::filesystem::path, boost::filesystem::path> Path;
    typedef std::vector<Path> Paths;
    ELLE_ATTRIBUTE(Paths, paths);

    typedef infinit::protocol::RPC<
      elle::serialize::InputBinaryArchive,
      elle::serialize::OutputBinaryArchive> RPC;
    RPC _rpc;
    RPC::RemoteProcedure<uint64_t> _rpc_count;
    RPC::RemoteProcedure<uint64_t> _rpc_full_size;
    RPC::RemoteProcedure<uint64_t, FileID> _rpc_file_size;
    RPC::RemoteProcedure<std::string,
                         FileID> _rpc_path;
    RPC::RemoteProcedure<infinit::cryptography::Code,
                         FileID,
                         Offset,
                         Size> _rpc_read;
    RPC::RemoteProcedure<void,
                         uint64_t> _rpc_set_progress;
    RPC::RemoteProcedure<elle::Version> _rpc_version;

    ELLE_ATTRIBUTE_RX(reactor::Signal, progress_changed);
  public:
    float
    progress() const;

    /*-----------------------.
    | Progress serialization |
    `-----------------------*/
  public:
    struct TransferSnapshot:
      public elle::Printable
    {
      // Recipient.
      TransferSnapshot(uint64_t count,
                       Size total_size);

      // Sender.
      TransferSnapshot();

      void
      progress(Size const& progress);

      void
      increment_progress(FileID index,
                         Size increment);

      void
      add(boost::filesystem::path const& root,
          boost::filesystem::path const& path);

      struct TransferInfo:
        public elle::Printable
      {
        TransferInfo(FileID file_id,
                     boost::filesystem::path const& root,
                     boost::filesystem::path const& path,
                     Size file_size);

        virtual
        ~TransferInfo() = default;

        ELLE_ATTRIBUTE_R(FileID, file_id);

        ELLE_ATTRIBUTE(std::string, root);
        ELLE_ATTRIBUTE_R(std::string, path);

        ELLE_ATTRIBUTE_R(boost::filesystem::path, full_path);
        ELLE_ATTRIBUTE_R(Size, file_size);

        bool
        file_exists() const;

        bool
        operator ==(TransferInfo const& rh) const;

        /*----------.
        | Printable |
        `----------*/
      public:
        virtual
        void
        print(std::ostream& stream) const;

        /*--------------.
        | Serialization |
        `--------------*/

        // XXX: Serialization require a default constructor when serializing
        // pairs...
        TransferInfo() = default;


        ELLE_SERIALIZE_CONSTRUCT(TransferInfo)
        {}

        ELLE_SERIALIZE_FRIEND_FOR(TransferInfo);

      };

      struct TransferProgressInfo:
        public TransferInfo
      {
      public:
        TransferProgressInfo(FileID file_id,
                             boost::filesystem::path const& root,
                             boost::filesystem::path const& path,
                             Size file_size);

        ELLE_ATTRIBUTE_R(Size, progress);

      private:
        void
        _increment_progress(Size increment);

      public:
        bool
        complete() const;

        bool
        operator ==(TransferProgressInfo const& rh) const;

        friend TransferSnapshot;

        /*----------.
        | Printable |
        `----------*/
      public:
        virtual
        void
        print(std::ostream& stream) const;

        /*--------------.
        | Serialization |
        `--------------*/

        TransferProgressInfo() = default;

        ELLE_SERIALIZE_CONSTRUCT(TransferProgressInfo, TransferInfo)
        {}

        ELLE_SERIALIZE_FRIEND_FOR(TransferProgressInfo);
      };

      ELLE_ATTRIBUTE_R(bool, sender);
      typedef std::unordered_map<FileID, TransferProgressInfo> TransferProgress;
      ELLE_ATTRIBUTE_X(TransferProgress, transfers);
      ELLE_ATTRIBUTE_R(uint64_t, count);
      ELLE_ATTRIBUTE_R(Size, total_size);
      ELLE_ATTRIBUTE_R(Size, progress);

      bool
      operator ==(TransferSnapshot const& rh) const;

      /*----------.
      | Printable |
      `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;

      /*--------------.
      | Serialization |
      `--------------*/

      ELLE_SERIALIZE_CONSTRUCT(TransferSnapshot)
      {}

      ELLE_SERIALIZE_FRIEND_FOR(TransferSnapshot);
    };

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

    ELLE_ATTRIBUTE(infinit::cryptography::SecretKey, key);
  };
}

#include <frete/Frete.hxx>

#endif
