#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_TROPHONIUS_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_TROPHONIUS_HH

# include <unordered_set>

# include <boost/date_time/posix_time/posix_time.hpp>

# include <elle/attribute.hh>

# include <reactor/network/tcp-server.hh>
# include <reactor/thread.hh>

# include <infinit/oracles/trophonius/server/fwd.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        class Trophonius:
          public elle::Printable
        {
        public:
          Trophonius(int port,
                     boost::posix_time::time_duration const& ping_period);
          ~Trophonius();

        /*-------.
        | Server |
        `-------*/
        public:
          int
          port() const;
        private:
          void
          _serve();
          ELLE_ATTRIBUTE(reactor::network::TCPServer, server);
          ELLE_ATTRIBUTE(reactor::Thread, accepter);

        /*--------.
        | Clients |
        `--------*/
        public:
          void
          client_remove(Client& c);
          ELLE_ATTRIBUTE(std::unordered_set<Client*>, clients);
          ELLE_ATTRIBUTE_R(boost::posix_time::time_duration, ping_period);

        /*----------.
        | Printable |
        `----------*/
        public:
          virtual
          void
          print(std::ostream& stream) const;
        };
      }
    }
  }
}

#endif
