#ifndef HOLE_IMPLEMENTATIONS_SLUG_SLUG_HH
# define HOLE_IMPLEMENTATIONS_SLUG_SLUG_HH

# include <reactor/duration.hh>
# include <reactor/network/Protocol.hh>
# include <reactor/network/socket.hh>
# include <reactor/network/udp-socket.hh>
# include <reactor/signal.hh>

# include <elle/network/Locus.hh>
# include <elle/types.hh>

# include <nucleus/proton/fwd.hh>

# include <hole/Hole.hh>
# include <hole/implementations/slug/fwd.hh>

namespace hole
{
  namespace implementations
  {
    namespace slug
    {

      /// Slug hole implementation.
      class Slug:
        public Hole,
        public elle::Printable
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Slug(hole::storage::Storage& storage,
             Passport const& passport,
             cryptography::PublicKey const& authority_K,
             reactor::network::Protocol protocol,
             std::vector<elle::network::Locus> const& members,
             int port,
             reactor::Duration connection_timeout,
             std::unique_ptr<reactor::network::UDPSocket> socket = nullptr);
        virtual ~Slug();
      private:
        ELLE_ATTRIBUTE_R(reactor::network::Protocol, protocol);
        ELLE_ATTRIBUTE_R(std::vector<elle::network::Locus>, members);
        ELLE_ATTRIBUTE_R(reactor::Duration, connection_timeout);

      /*------.
      | State |
      `------*/
      public:
        enum class State
        {
          detached,
          attached,
        };
      private:
        State _state;

      /*---------------.
      | Implementation |
      `---------------*/
      protected:
        virtual
        void
        _push(const nucleus::proton::Address& address,
              const nucleus::proton::ImmutableBlock& block);
        virtual
        void
        _push(const nucleus::proton::Address& address,
              const nucleus::proton::MutableBlock& block);
        virtual
        std::unique_ptr<nucleus::proton::Block>
        _pull(const nucleus::proton::Address&);
        virtual
        std::unique_ptr<nucleus::proton::Block>
        _pull(const nucleus::proton::Address&,
              const nucleus::proton::Revision&);
        virtual
        void
        _wipe(const nucleus::proton::Address& address);
      private:
        std::unique_ptr<nucleus::proton::Block>
        _get_latest(nucleus::proton::Address const&);
        std::unique_ptr<nucleus::proton::Block>
        _get_specific(nucleus::proton::Address const&, nucleus::proton::Revision const&);


      /*------.
      | Hosts |
      `------*/
      public:
        std::vector<elle::network::Locus> loci();
        std::vector<Host*> hosts();
      private:
        friend class Host;
        /// XXX We need to stop storing naked pointer.
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
        void
        _remove(Host* host);
        void
        _remove(elle::network::Locus loc);
        Hosts _hosts;
        Hosts _pending;

        reactor::Signal _new_host;

      /*-------.
      | Server |
      `-------*/
      public:
        ELLE_ATTRIBUTE_R(elle::network::Port,  port);
      private:
        void _accept();
        std::unique_ptr<reactor::network::Server> _server;
        std::unique_ptr<reactor::Thread> _acceptor;

      /*-------.
      | Portal |
      `-------*/
      public:
        void portal_connect(std::string const& host, int port);
        bool portal_wait(std::string const& host, int port);


      /*----------.
      | Printable |
      `----------*/

      public:
        virtual
        void
        print(std::ostream& stream) const;

      /*---------.
      | Dumpable |
      `---------*/
      public:
        elle::Status
        Dump(const elle::Natural32) const;
      };


      std::ostream&
      operator << (std::ostream& stream, Slug::State state);
    }
  }
}

#endif
