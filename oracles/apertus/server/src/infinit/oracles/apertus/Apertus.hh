#ifndef ORACLES_APERTUS_APERTUS
# define ORACLES_APERTUS_APERTUS

# include <string>

# include <boost/uuid/uuid.hpp>

# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <infinit/oracles/meta/Admin.hh>
# include <infinit/oracles/hermes/Clerk.hh>

namespace oracles
{
  namespace apertus
  {
    class Connection
    {
    public:
      Connection(reactor::network::TCPSocket* sock);

    private:
      reactor::network::TCPSocket* _sock;
    };

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
      reactor::Scheduler& _sched;

    private:
      infinit::oracles::meta::Admin _meta;
      const boost::uuids::uuid _uuid;

    private:
      const std::string _host;
      const int _port;

    private:
      std::map<oracle::hermes::TID, Connection> _clients;
    };
  }
}

#endif // !ORACLES_APERTUS_APERTUS
