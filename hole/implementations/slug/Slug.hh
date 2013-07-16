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
      class AlreadyConnected:
        public elle::Exception
      {
      public:
        AlreadyConnected();
      };


      /// Slug hole implementation.
      class Slug:
        public Hole
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        typedef std::vector<elle::network::Locus> Members;
        Slug(hole::storage::Storage& storage,
             elle::Passport const& passport,
             elle::Authority const& authority,
             reactor::network::Protocol protocol,
             Members const& members = Members(),
             int port = 0,
             reactor::Duration connection_timeout =
               boost::posix_time::milliseconds(5000),
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
      private:
        friend class Host;
        typedef std::unordered_map<elle::Passport,
                                   std::shared_ptr<Host>> Hosts;
        void
        _host_register(std::shared_ptr<Host> host);
        std::shared_ptr<Host>
        _connect(elle::network::Locus const& locus);
        std::shared_ptr<Host>
        _connect(std::unique_ptr<reactor::network::Socket> socket,
                 elle::network::Locus const& locus, bool opener);
        void
        _connect_try(elle::network::Locus const& locus);
        void
        _remove(Host* host);

        bool
        _host_connected(elle::Passport const& passport);
        std::shared_ptr<Host>
        _host_pending(elle::Passport const& passport);
        bool
        _host_wait(std::shared_ptr<Host> host);

        /// Authenticated hosts.
        ELLE_ATTRIBUTE_R(Hosts, hosts);
        /// Not-yet authenticated hosts.
        ELLE_ATTRIBUTE_R(std::set<std::shared_ptr<Host>>, pending);
        /// Signal that a new host was registered.
        ELLE_ATTRIBUTE_RX(reactor::Signal, new_host);
        ELLE_ATTRIBUTE_RX(reactor::Signal, new_connected_host);

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
        void portal_connect(std::string const& host, int port, bool server);

      /*----------.
      | Printable |
      `----------*/

      public:
        virtual
        void
        print(std::ostream& stream) const override;

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
