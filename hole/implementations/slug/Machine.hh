#ifndef HOLE_IMPLEMENTATIONS_SLUG_MACHINE_HH
# define HOLE_IMPLEMENTATIONS_SLUG_MACHINE_HH

# include <unordered_map>

# include <elle/attribute.hh>
# include <elle/network/Locus.hh>
# include <elle/types.hh>

# include <reactor/network/fwd.hh>
# include <reactor/duration.hh>
# include <reactor/signal.hh>
# include <reactor/network/udt-server.hh>
# include <reactor/network/udt-socket.hh>

# include <hole/fwd.hh>

# include <nucleus/fwd.hh>

# include <lune/fwd.hh>

# include <hole/implementations/slug/fwd.hh>

# include <boost/signals.hpp>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {
      void portal_connect(std::string const& host, int port);
      bool portal_wait(std::string const& host, int port);
      void portal_host_authenticated(elle::network::Locus const& locus);
      void portal_machine_authenticated(elle::network::Locus const& locus);

      ///
      /// XXX represents the current host
      ///
      class Machine:
        public elle::Printable
      {
      public:
        // constants
        static const reactor::Duration  Timeout;

      /*-------------.
      | Construction |
      `-------------*/
      public:
        Machine(Implementation& hole,
                int port,
                reactor::Duration connection_timeout);
        ~Machine();
        ELLE_ATTRIBUTE_RX(Implementation&, hole);
        ELLE_ATTRIBUTE_RX(reactor::Duration, connection_timeout);

      /*------.
      | State |
      `------*/
      public:
        enum class State
        {
          detached,
          attached
        };
      private:
        State _state;

      /*------.
      | Hosts |
      `------*/
      public:
        std::vector<elle::network::Locus> loci();
        std::vector<Host*> hosts();
      private:
        friend class Host;
        typedef std::unordered_map<elle::network::Locus, Host*> Hosts;
        void
        _host_register(Host* host);
        void
        _connect(elle::network::Locus const& locus);
        void
        _connect(std::unique_ptr<reactor::network::Socket> socket,
                 elle::network::Locus const& locus, bool opener);
        void
        _connect_try(elle::network::Locus const& locus);
        void _remove(Host* host);
        Hosts _hosts;

      /*-------.
      | Server |
      `-------*/
      public:
        elle::network::Port port() const;
        // XXX
        void portal_connect(std::string const& host, int port);
        bool portal_wait(std::string const& host, int port);
      private:
        elle::network::Port _port;
        void _accept();
        std::unique_ptr<reactor::network::Server> _server;
        std::unique_ptr<reactor::Thread> _acceptor;

      /*----.
      | API |
      `----*/
      public:
        /// Store an immutable block.
        void
        put(const nucleus::proton::Address&,
            const nucleus::proton::ImmutableBlock&);
        /// Store a mutable block.
        void
        put(const nucleus::proton::Address&,
            const nucleus::proton::MutableBlock&);
        /// Retrieves an immutable block.
        std::unique_ptr<nucleus::proton::Block>
        get(const nucleus::proton::Address&);
        /// Retrieves a mutable block.
        std::unique_ptr<nucleus::proton::Block>
        get(const nucleus::proton::Address&,
            const nucleus::proton::Revision&);
        /// Remove a block.
        void
        wipe(const nucleus::proton::Address&);
      private:
        std::unique_ptr<nucleus::proton::Block>
        _get_latest(nucleus::proton::Address const&);
        std::unique_ptr<nucleus::proton::Block>
        _get_specific(nucleus::proton::Address const&, nucleus::proton::Revision const&);

      /*----------.
      | Printable |
      `----------*/
      public:
        virtual void print(std::ostream& s) const;

      /*---------.
      | Dumpable |
      `---------*/
      public:
        elle::Status            Dump(const elle::Natural32 = 0) const;
      };

      std::ostream&
      operator << (std::ostream& stream, Machine::State state);
    }
  }
}

#endif
