#ifndef INFINIT_ORACLES_APERTUS_APERTUS
# define INFINIT_ORACLES_APERTUS_APERTUS

# include <string>
# include <map>
# include <unordered_map>

# include <boost/uuid/uuid.hpp>
# include <boost/uuid/random_generator.hpp>

# include <reactor/waitable.hh>

# include <reactor/network/buffer.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <reactor/operation.hh>
# include <infinit/oracles/meta/Admin.hh>
# include <infinit/oracles/hermes/Clerk.hh>

# include <infinit/oracles/apertus/fwd.hh>

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      class Apertus :
        public reactor::Waitable
      {
      public:
        Apertus(std::string mhost,
                int mport,
                std::string host = "0.0.0.0",
                int port = 6565);
        ~Apertus();

      private:
        void
        _register();

        void
        _unregister();

      public:
        void
        stop();

        std::map<oracle::hermes::TID, reactor::network::TCPSocket*>&
        get_clients();

      private:
        void
        _connect(oracle::hermes::TID tid,
                 std::unique_ptr<reactor::network::TCPSocket> client1,
                 std::unique_ptr<reactor::network::TCPSocket> client2);

        void
        _run();

      private:
        reactor::Thread _accepter;

      private:
        ELLE_ATTRIBUTE(infinit::oracles::meta::Admin, meta);
        const boost::uuids::uuid _uuid;
        typedef std::unordered_set<std::unique_ptr<Accepter>> Accepters;
        ELLE_ATTRIBUTE(Accepters, accepters);

        friend Accepter;
        friend Transfer;

        void
        _transfer_remove(Transfer const& transfer);

        void
        _accepter_remove(Accepter const& transfer);

        typedef std::unordered_map<oracle::hermes::TID, std::unique_ptr<Transfer>> Workers;
        ELLE_ATTRIBUTE(Workers, workers);

      private:
        const std::string _host;
        const int _port;

      private:
        typedef std::map<oracle::hermes::TID, reactor::network::TCPSocket*> Clients;
        ELLE_ATTRIBUTE_R(Clients, clients);

      private:
      };
    }
  }
}

#endif // INFINIT_ORACLES_APERTUS_APERTUS
