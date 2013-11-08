#ifndef ORACLES_APERTUS_APERTUS
# define ORACLES_APERTUS_APERTUS

# include <string>

# include <boost/uuid/uuid.hpp>

# include <reactor/network/buffer.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <infinit/oracles/meta/Admin.hh>
# include <infinit/oracles/hermes/Clerk.hh>

namespace oracles
{
  namespace apertus
  {
    class Apertus
    {
    public:
      Apertus(reactor::Scheduler& sched,
              std::string mhost, int mport,
              std::string host = "0.0.0.0", int port = 6565);
      ~Apertus();

      void
      run();

    private:
      void
      _connect(reactor::network::TCPSocket* client1,
               reactor::network::TCPSocket* client2);

    private:
      reactor::Scheduler& _sched;

    private:
      infinit::oracles::meta::Admin _meta;
      const boost::uuids::uuid _uuid;

    private:
      const std::string _host;
      const int _port;

    private:
      std::map<oracle::hermes::TID, reactor::network::TCPSocket*> _clients;
    };
  }
}

#endif // !ORACLES_APERTUS_APERTUS
