#ifndef ORACLE_DISCIPLES_HERMES_HH
# define ORACLE_DISCIPLES_HERMES_HH

# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <reactor/scheduler.hh>

# include <protocol/ChanneledStream.hh>
# include <protocol/RPC.hh>
# include <protocol/Serializer.hh>

# include <elle/serialize/BinaryArchive.hh>
# include <elle/Buffer.hh>

# include <oracle/disciples/hermes/src/hermes/Clerk.hh>

namespace oracle
{
  namespace hermes
  {
    class Hermes
    {
    public:
      Hermes(reactor::Scheduler& sched, int port, std::string base_path);
      ~Hermes();

    public:
      void
      run();

    private:
      reactor::Scheduler& _sched;
      reactor::network::TCPServer _serv;
      std::vector<reactor::Thread*> _clients;

    private:
      boost::filesystem::path _base_path;
      int _port;
    };

    class HermesRPC:
      public infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                    elle::serialize::OutputBinaryArchive>
    {
    public:
      HermesRPC(infinit::protocol::ChanneledStream& channels);
      RemoteProcedure<void, TID> ident;
      RemoteProcedure<Size, FileID, Offset, elle::Buffer&> store;
      RemoteProcedure<elle::Buffer, FileID, Offset, Size> fetch;
    };
  }
}

#endif // !ORACLE_DISCIPLES_HERMES_HH
