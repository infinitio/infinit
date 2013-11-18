#ifndef INFINIT_ORACLES_APERTUS_APERTUS
# define INFINIT_ORACLES_APERTUS_APERTUS

# include <infinit/oracles/apertus/fwd.hh>
# include <infinit/oracles/hermes/Clerk.hh>
# include <infinit/oracles/meta/Admin.hh>

# include <reactor/network/buffer.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/network/tcp-socket.hh>
# include <reactor/operation.hh>
# include <reactor/waitable.hh>

# include <elle/Printable.hh>

# include <boost/uuid/random_generator.hpp>
# include <boost/uuid/uuid.hpp>

# include <map>
# include <string>
# include <unordered_map>


namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      class Apertus:
        public reactor::Waitable // Make it also printable.
      {
      public:
        Apertus(std::string mhost,
                int mport,
                std::string host = "0.0.0.0",
                int port = 6565,
                std::time_t tick_rate = 10);
        ~Apertus();

      private:
        void
        _register();

        void
        _unregister();

      public:
        void
        stop();

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
        ELLE_ATTRIBUTE(boost::uuids::uuid, uuid);
        typedef std::unordered_set<Accepter*> Accepters;
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
        int _port;

      private:
        typedef std::map<oracle::hermes::TID, reactor::network::TCPSocket*> Clients;
        ELLE_ATTRIBUTE_R(Clients, clients);

        /*----------.
        | Printable |
        `----------*/
        virtual
        void
        print(std::ostream& stream) const;

        /*-----------.
        | Monitoring |
        `-----------*/
      public:
        void
        refresh_bandwidth(uint32_t data);

        uint32_t
        get_bandwidth();

      private:
        ELLE_ATTRIBUTE(std::time_t, tick_rate);
        ELLE_ATTRIBUTE(std::time_t, last_tick);
        ELLE_ATTRIBUTE(uint32_t, bandwidth);
      };
    }
  }
}

#endif // INFINIT_ORACLES_APERTUS_APERTUS
