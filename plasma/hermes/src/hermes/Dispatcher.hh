#ifndef PLASMA_HERMES_DISPATCHER_HH
# define PLASMA_HERMES_DISPATCHER_HH

# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <reactor/scheduler.hh>

# include <protocol/ChanneledStream.hh>
# include <protocol/RPC.hh>
# include <protocol/Serializer.hh>

# include <elle/serialize/BinaryArchive.hh>
# include <elle/Buffer.hh>

# include <plasma/hermes/src/hermes/Clerk.hh>

namespace plasma
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

    private:
      static Size store_wrapper(FileID id, Offset off, elle::Buffer& buff);
      static elle::Buffer serve_wrapper(FileID id, Offset off);
    };
  }
}

#endif // !PLASMA_HERMES_DISPATCHER_HH
