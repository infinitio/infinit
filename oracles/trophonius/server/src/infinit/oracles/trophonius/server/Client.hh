#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH

# include <memory>

# include <boost/uuid/uuid.hpp>

# include <elle/attribute.hh>

# include <reactor/network/tcp-socket.hh>
# include <reactor/thread.hh>
# include <reactor/Barrier.hh>

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

        private:
          void
          handle();

          virtual
          // This function is a automaticly warded.
          void
          _handle() = 0;

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
