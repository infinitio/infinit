#include <oracle/disciples/hermes/src/hermes/Dispatcher.hh>
// TODO: Correct include path.

namespace oracle
{
  namespace hermes
  {
    Dispatcher::Dispatcher(reactor::Scheduler& sched, Clerk& clerk, int port):
      _sched(sched),
      _serv(sched),
      _clerk(clerk),
      _port(port)
    {}

    void
    Dispatcher::run()
    {
      _serv.listen(_port);

      // Run an RPC at each connection.
      while (true)
      {
        // TODO: Handle both Alice and Bob.
        infinit::protocol::Serializer s(_sched, *_serv.accept());
        infinit::protocol::ChanneledStream channels(_sched, s);

        Handler rpc(channels);
        rpc.store = std::bind(&Clerk::store,
                              &_clerk,
                              std::placeholders::_1,
                              std::placeholders::_2,
                              std::placeholders::_3);
        rpc.serve = std::bind(&Clerk::serve,
                              &_clerk,
                              std::placeholders::_1,
                              std::placeholders::_2);
        rpc.run();
      }
    }

    Handler::Handler(infinit::protocol::ChanneledStream& channels):
      infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                             elle::serialize::OutputBinaryArchive>(channels),
      store("store", *this),
      serve("serve", *this)
    {}
  }
}

