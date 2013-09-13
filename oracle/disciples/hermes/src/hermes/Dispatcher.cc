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

    Dispatcher::~Dispatcher()
    {
      for (auto client : _clients)
        if (client != nullptr)
        {
          client->terminate_now();
          delete client;
        }
    }

    void
    Dispatcher::run()
    {
      _serv.listen(_port);

      // Run an RPC at each connection.
      while (true)
      {
        reactor::network::TCPSocket* sock = _serv.accept();

        auto client = [=] ()
        {
          infinit::protocol::Serializer s(_sched, *sock);
          infinit::protocol::ChanneledStream channels(_sched, s);

          Handler rpc(channels);

          rpc.store = std::bind(&Clerk::store,
                                &_clerk,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);
          rpc.fetch = std::bind(&Clerk::fetch,
                                &_clerk,
                                std::placeholders::_1,
                                std::placeholders::_2);

          rpc.run();
        };

        // TODO: Change name of clients.
        _clients.emplace_back(new reactor::Thread(_sched, "random", client));
      }
    }

    Handler::Handler(infinit::protocol::ChanneledStream& channels):
      infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                             elle::serialize::OutputBinaryArchive>(channels),
      store("store", *this),
      fetch("fetch", *this)
    {}
  }
}

