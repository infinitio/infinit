#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH

# include <memory>

# include <boost/uuid/uuid.hpp>

# include <elle/attribute.hh>

# include <reactor/network/tcp-socket.hh>
# include <reactor/thread.hh>

# include <infinit/oracles/trophonius/server/fwd.hh>
# include <infinit/oracles/meta/Admin.hh>

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

        public:
          void
          terminate();
          void
          unregister();

        private:
          void
          handle();

        protected:
          virtual
          void
          _handle() = 0;
          virtual
          void
          _terminate();
          virtual
          void
          _unregister();

          ELLE_ATTRIBUTE_R(Trophonius&, trophonius);
        protected:
          std::unique_ptr<reactor::network::TCPSocket> _socket;

          ELLE_ATTRIBUTE(reactor::Thread, handle_thread);

        /*-----.
        | Ward |
        `-----*/
        public:
          class RemoveWard:
            public elle::SafeFinally
          {
          public:
            RemoveWard(Client& c);
          };

        /*----------.
        | Printable |
        `----------*/
        public:
          void
          print(std::ostream& stream) const override;
        };
      }
    }
  }
}

#endif
