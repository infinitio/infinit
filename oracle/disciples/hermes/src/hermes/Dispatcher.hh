#ifndef ORACLE_DISCIPLES_HERMES_DISPATCHER_HH
# define ORACLE_DISCIPLES_HERMES_DISPATCHER_HH

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
    class Dispatcher
    {
    public:
      Dispatcher(reactor::Scheduler& sched, Clerk& clerk, int port);
      void run();

    private:
      reactor::Scheduler& _sched;
      reactor::network::TCPServer _serv;

    private:
      Clerk& _clerk;
      int _port;
    };

    class Handler:
      public infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                    elle::serialize::OutputBinaryArchive>
    {
    public:
      Handler(infinit::protocol::ChanneledStream& channels);
      RemoteProcedure<Size, FileID, Offset, elle::Buffer&> store;
      RemoteProcedure<elle::Buffer, FileID, Offset> serve;
    };
  }
}

#endif // !ORACLE_DISCIPLES_HERMES_DISPATCHER_HH
