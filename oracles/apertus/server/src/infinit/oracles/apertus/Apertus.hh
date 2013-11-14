#ifndef ORACLES_APERTUS_APERTUS
# define ORACLES_APERTUS_APERTUS

# include <string>

# include <boost/uuid/uuid.hpp>
# include <boost/uuid/random_generator.hpp>

# include <reactor/waitable.hh>

# include <reactor/network/buffer.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <infinit/oracles/meta/Admin.hh>
# include <infinit/oracles/hermes/Clerk.hh>

namespace oracles
{
  namespace apertus
  {
    class Apertus :
      public reactor::Waitable
    {
    public:
      Apertus(reactor::Scheduler& sched,
              std::string mhost, int mport,
              std::string host = "0.0.0.0", int port = 6565);
      ~Apertus();

      void
      reg();

      void
      unreg();

      void
      stop();

      std::map<oracle::hermes::TID, reactor::network::TCPSocket*>&
      get_clients();

    private:
      void
      _connect(reactor::network::TCPSocket* client1,
               reactor::network::TCPSocket* client2);

      void
      _run();

    private:
      reactor::Scheduler& _sched;
      reactor::Thread _accepter;

    private:
      infinit::oracles::meta::Admin* _meta;
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
