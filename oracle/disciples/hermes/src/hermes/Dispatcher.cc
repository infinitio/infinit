#include <oracle/disciples/hermes/src/hermes/Dispatcher.hh>
// TODO: Correct include path.

namespace oracle
{
  namespace hermes
  {
    Dispatcher::Dispatcher(reactor::Scheduler& sched, int port, std::string p):
      _sched(sched),
      _serv(sched),
      _path(p),
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

          HermesRPC rpc(channels);
          Clerk clerk(_path);

          rpc.ident = std::bind(&Clerk::ident,
                                &clerk,
                                std::placeholders::_1);

          rpc.store = std::bind(&Clerk::store,
                                &clerk,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);

          rpc.fetch = std::bind(&Clerk::fetch,
                                &clerk,
                                std::placeholders::_1,
                                std::placeholders::_2);

          rpc.run();
        };

        // TODO: Change name of clients.
        _clients.emplace_back(new reactor::Thread(_sched, "random", client));
      }
    }

    HermesRPC::HermesRPC(infinit::protocol::ChanneledStream& channels):
      infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                             elle::serialize::OutputBinaryArchive>(channels),
      ident("ident", *this),
      store("store", *this),
      fetch("fetch", *this)
    {}
  }
}

