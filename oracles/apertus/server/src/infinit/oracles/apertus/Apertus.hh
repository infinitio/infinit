#ifndef INFINIT_ORACLES_APERTUS_APERTUS
# define INFINIT_ORACLES_APERTUS_APERTUS

# include <infinit/oracles/apertus/fwd.hh>
# include <infinit/oracles/apertus/Accepter.hh>
# include <infinit/oracles/meta/Admin.hh>

# include <reactor/network/buffer.hh>
# include <reactor/network/socket.hh>
# include <reactor/network/ssl-server.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/operation.hh>
# include <reactor/waitable.hh>

# include <elle/Printable.hh>

# include <boost/date_time/posix_time/posix_time.hpp>
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
        typedef std::string TID;

      public:
        Apertus(std::string const& meta_protocol,
                std::string const& meta_host,
                int meta_port,
                std::string const& host = "0.0.0.0",
                int port_ssl = 6566,
                int port_tcp = 6565,
                boost::posix_time::time_duration const& tick_rate = 10_sec,
                boost::posix_time::time_duration const& timeout = 5_min
                );
        ~Apertus();

      private:

        void
        _register();

        void
        _unregister();

        ELLE_ATTRIBUTE(bool, unregistered);

      public:
        void
        stop();

      private:
        void
        _connect(TID tid,
                 std::unique_ptr<reactor::network::Socket> client1,
                 std::unique_ptr<reactor::network::Socket> client2);

        void
        _serve(reactor::network::Server& server);

      private:
        ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter_ssl);
        ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter_tcp);

      private:
        typedef std::unique_ptr<Accepter> AccepterPtr;
        ELLE_ATTRIBUTE(bool, meta_enabled);
        ELLE_ATTRIBUTE(infinit::oracles::meta::Admin, meta);
        ELLE_ATTRIBUTE(boost::uuids::uuid, uuid);
        typedef std::unordered_map<Accepter*, AccepterPtr> Accepters;
        // just connected
        ELLE_ATTRIBUTE_R(Accepters, accepters);

        AccepterPtr
        _take_from_accepters(Accepter*);
        ELLE_ATTRIBUTE(bool, stop_ordered);

        friend Accepter;
        friend Transfer;

        void
        _transfer_remove(Transfer const& transfer);

        typedef std::unordered_map<TID, std::unique_ptr<Transfer>> Workers;
        ELLE_ATTRIBUTE_R(Workers, workers);

      private:
        const std::string _host;
        ELLE_ATTRIBUTE_R(int, port_ssl);
        ELLE_ATTRIBUTE_R(int, port_tcp);
        ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::SSLCertificate>,
                       certificate);
        ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::SSLServer>,
                       server_ssl);
        ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::TCPServer>,
                       server_tcp);

      private:
        typedef std::unordered_map<TID, AccepterPtr> Clients;
        // id received, not associated
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
        add_to_bandwidth(uint32_t data);

      private:
        void
        _run_monitor();

      private:
        ELLE_ATTRIBUTE(uint32_t, bandwidth);
        ELLE_ATTRIBUTE(boost::posix_time::time_duration, tick_rate);
        ELLE_ATTRIBUTE(boost::posix_time::time_duration, timeout);
        ELLE_ATTRIBUTE(reactor::Thread, monitor);
      };
    }
  }
}

#endif // INFINIT_ORACLES_APERTUS_APERTUS
