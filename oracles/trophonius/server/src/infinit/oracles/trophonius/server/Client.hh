#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH

# include <memory>

# include <elle/attribute.hh>

# include <reactor/network/tcp-socket.hh>
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
        class Client:
          public elle::Printable
        {
        public:
          Client(Trophonius& trophonius,
                 std::unique_ptr<reactor::network::TCPSocket>&& socket);
          virtual
          ~Client();

        private:
          void
          _handle();
          ELLE_ATTRIBUTE_R(Trophonius&, trophonius);
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::TCPSocket>, socket);
          ELLE_ATTRIBUTE(reactor::Thread, handle_thread);
          ELLE_ATTRIBUTE_R(bool, authentified);

        /*----------.
        | Ping pong |
        `----------*/
        public:
          void
          _ping();
          void
          _pong();
          ELLE_ATTRIBUTE(reactor::Thread, ping_thread);
          ELLE_ATTRIBUTE(reactor::Thread, pong_thread);
          ELLE_ATTRIBUTE_R(bool, pinged);

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
