#ifndef FRETE_FRETE_HH
# define FRETE_FRETE_HH

# include <ios>
# include <stdint.h>
# include <tuple>
# include <algorithm>

# include <boost/filesystem.hpp>

# include <elle/serialize/BinaryArchive.hh>
# include <elle/Buffer.hh>
# include <elle/container/map.hh>

# include <protocol/ChanneledStream.hh>
# include <protocol/Serializer.hh>
# include <protocol/RPC.hh>

namespace frete
{
  class Frete
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
    get(boost::filesystem::path const& output);

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
    elle::Buffer
    _read(FileID f, Offset start, Size size);
    void
    _set_progress(uint64_t progress);

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
    RPC::RemoteProcedure<elle::Buffer,
                         FileID,
                         Offset,
                         Size> _rpc_read;
    RPC::RemoteProcedure<void,
                         uint64_t> _rpc_set_progress;

    ELLE_ATTRIBUTE_RX(reactor::Signal, progress_changed);
  public:
    float
    progress() const;

    /*-----------------------.
    | Progress serialization |
    `-----------------------*/
  private:
    ELLE_ATTRIBUTE(boost::filesystem::path, snapshot_destination);

  public:
    struct TransferSnapshot:
      public elle::Printable
    {
      ELLE_SERIALIZE_CONSTRUCT(TransferSnapshot)
      {}

      ELLE_SERIALIZE_FRIEND_FOR(TransferSnapshot);

      struct TransferInfo:
        public elle::Printable
      {
        ELLE_SERIALIZE_CONSTRUCT(TransferInfo)
        {}

        ELLE_SERIALIZE_FRIEND_FOR(TransferInfo);

        TransferInfo(FileID file_id,
                     boost::filesystem::path const& root,
                     boost::filesystem::path const& path,
                     Size file_size):
          _file_id(file_id),
          _root(root.string()),
          _path(path.string()),
          _full_path(root / path),
          _file_size(file_size)
        {}

        // XXX: Serialization require a default constructor when serializing
        // pairs...
        TransferInfo() = default;

        virtual
        ~TransferInfo() = default;

        ELLE_ATTRIBUTE_R(FileID, file_id);

        ELLE_ATTRIBUTE(std::string, root);
        ELLE_ATTRIBUTE_R(std::string, path);

        ELLE_ATTRIBUTE_R(boost::filesystem::path, full_path);
        ELLE_ATTRIBUTE_R(Size, file_size);

        bool
        file_exists() const
        {
          return boost::filesystem::exists(this->_full_path);
        }

        /*----------.
        | Printable |
        `----------*/
      public:
        virtual
        void
        print(std::ostream& stream) const
        {
          stream << "TransferInfo "
                 << this->_file_id << " : " << this->_full_path << "(" << this->_file_size << ")";
        }

        bool
        operator ==(TransferInfo const& rh) const
        {
          return ((this->_file_id == rh._file_id) &&
                  (this->_full_path == rh._full_path) &&
                  (this->_file_size == rh._file_size));
        }
      };

      struct TransferProgressInfo:
        public TransferInfo
      {
        ELLE_SERIALIZE_CONSTRUCT(TransferProgressInfo, TransferInfo)
        {}

        ELLE_SERIALIZE_FRIEND_FOR(TransferProgressInfo);

      public:
        TransferProgressInfo(FileID file_id,
                             boost::filesystem::path const& root,
                             boost::filesystem::path const& path,
                             Size file_size):
          TransferInfo(file_id, root, path, file_size),
          _progress{0}
        {}

        TransferProgressInfo() = default;

        friend TransferSnapshot;

        ELLE_ATTRIBUTE_R(Size, progress);

      private:
        void
        _increment_progress(Size increment)
        {
          this->_progress += increment;
        }

      public:
        bool
        complete() const
        {
          return (this->file_size() == this->_progress);
        }

        /*----------.
        | Printable |
        `----------*/
      public:
        virtual
        void
        print(std::ostream& stream) const
        {
          stream << "ProgressInfo "
                 << this->file_id() << " : " << this->full_path() << "(" << this->_progress << " / " << this->file_size() << ")";
        }

        bool
        operator ==(TransferProgressInfo const& rh) const
        {
          return ((this->TransferInfo::operator ==(rh) &&
                   this->_progress == rh._progress));
        }
      };

      // Recipient.
      TransferSnapshot(uint64_t count,
                       Size total_size):
        _sender(false),
        _count(count),
        _total_size(total_size),
        _progress{0}
      {
        ELLE_LOG_COMPONENT("frete");
        ELLE_LOG("%s: contruction", *this);
      }

      // Sender.
      TransferSnapshot():
        _sender(true),
        _count(0),
        _total_size(0),
        _progress{0}
      {
        ELLE_LOG_COMPONENT("frete");
        ELLE_LOG("%s: contruction", *this);
      }

      void
      increment_progress(FileID index,
                         Size increment)
      {
        ELLE_ASSERT_LT(index, this->_transfers.size());

        this->_transfers.at(index)._increment_progress(increment);
        this->_progress += increment;
      }

      void
      add(boost::filesystem::path const& root,
          boost::filesystem::path const& path)
      {
        ELLE_ASSERT(this->_sender);

        auto file = root / path;
        if (!boost::filesystem::exists(file))
          throw elle::Exception(elle::sprintf("file %s doesn't exist", file));

        auto index = this->_transfers.size();
        auto size = boost::filesystem::file_size(file);
        this->_transfers.emplace(
          std::piecewise_construct,
          std::make_tuple(index),
          std::forward_as_tuple(index, root, path, size));
        this->_total_size += size;
        this->_count = this->_transfers.size();
      }

      ELLE_ATTRIBUTE_R(bool, sender);
      typedef std::map<FileID, TransferProgressInfo> TransferProgress;
      ELLE_ATTRIBUTE_X(TransferProgress, transfers);
      ELLE_ATTRIBUTE_R(uint64_t, count);
      ELLE_ATTRIBUTE_R(Size, total_size);
      ELLE_ATTRIBUTE_R(Size, progress);

      bool
      operator ==(TransferSnapshot const& rh) const
      {
        return ((this->_count == rh._count) &&
                (this->_total_size == rh._total_size) &&
                (this->_progress == rh._progress) &&
                std::equal(this->_transfers.begin(),
                           this->_transfers.end(),
                           rh._transfers.begin()));
      }

      void
      progress(Size const& bite)
      {
        ELLE_LOG_COMPONENT("frete");

        ELLE_ASSERT(this->sender());

        if (this->_progress < bite)
          ELLE_WARN("%s: reducing progress", *this);

        this->_progress = bite;
      }

      /*----------.
      | Printable |
      `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const
      {
        stream << "Snapshot " << (this->_sender ? "sender" : "recipient")
               << " " << this->_count << " files for a total size of" << this->_total_size
               << ". Already 'dl': " << this->_progress
               << this->_transfers;
      }

    };

    ELLE_ATTRIBUTE(std::unique_ptr<TransferSnapshot>, transfer_snapshot);
  };
}

#include <frete/Frete.hxx>

#endif
