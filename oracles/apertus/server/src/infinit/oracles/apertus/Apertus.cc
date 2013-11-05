#include <infinit/oracles/apertus/Apertus.hh>

namespace oracles
{
  namespace apertus
  {
    Apertus::Apertus(reactor::Scheduler& sched,
                     std::string mhost, int mport,
                     std::string host, int port):
      _sched(sched),
      _meta(mhost, mport),
      _uuid(),
      _host(host),
      _port(port)
    {
      _meta.register_apertus(_uuid, port);
    }

    Apertus::~Apertus()
    {
      _meta.unregister_apertus(_uuid);
    }

    void
    Apertus::run()
    {
      reactor::network::TCPServer serv(_sched);
      serv.listen(_port);

      reactor::network::TCPSocket* client = nullptr;
      while ((client = serv.accept()) != nullptr)
      {}
    }

    Connection::Connection(reactor::network::TCPSocket* sock):
      _sock(sock)
    {}
  }
}
