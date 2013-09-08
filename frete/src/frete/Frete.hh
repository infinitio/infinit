#ifndef FRETE_FRETE_HH
# define FRETE_FRETE_HH

# include <ios>

# include <boost/filesystem.hpp>

# include <elle/serialize/BinaryArchive.hh>
# include <elle/Buffer.hh>

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
    Frete(infinit::protocol::ChanneledStream& channels);
    ~Frete();

  /*--------------------.
  | Local configuration |
  `--------------------*/
  public:
    void
    add(boost::filesystem::path const& path);
    void
    add(boost::filesystem::path const& root,
        boost::filesystem::path const& path);

  /*-------------.
  | Remote calls |
  `-------------*/
  public:
    /// The number of file in the remote filesystem.
    uint64_t
    size();
    /// The path of the \a f file.
    std::string
    path(FileID f);
    /// A chunk of a remote file.
    elle::Buffer
    read(FileID f, Offset start, Size size);

  /*-----.
  | RPCs |
  `-----*/
  private:
    uint64_t
    _size();
    std::string
    _path(FileID f);
    elle::Buffer
    _read(FileID f, Offset start, Size size);
    typedef std::pair<boost::filesystem::path, boost::filesystem::path> Path;
    typedef std::vector<Path> Paths;
    ELLE_ATTRIBUTE(Paths, paths);
    typedef infinit::protocol::RPC<
      elle::serialize::InputBinaryArchive,
      elle::serialize::OutputBinaryArchive> RPC;
    RPC _rpc;
    RPC::RemoteProcedure<uint64_t> _rpc_size;
    RPC::RemoteProcedure<std::string,
                         FileID> _rpc_path;
    RPC::RemoteProcedure<elle::Buffer,
                         FileID,
                         Offset,
                         Size> _rpc_read;
    reactor::Thread _rpc_thread;
  };
}





#endif
