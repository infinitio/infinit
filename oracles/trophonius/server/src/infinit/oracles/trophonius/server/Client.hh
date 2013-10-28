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

        /*----------.
        | Printable |
        `----------*/
        public:
          void
          print(std::ostream& stream) const override;
        };

        class Meta:
          public Client
        {
        public:
          Meta(Trophonius& trophonius,
               std::unique_ptr<reactor::network::TCPSocket>&& socket);

          void
          _handle() override;

        /*----------.
        | Printable |
        `----------*/
        public:
          void
          print(std::ostream& stream) const override;
        };

        class User:
          public Client
        {
          friend Meta;

        public:
          User(Trophonius& trophonius,
               std::unique_ptr<reactor::network::TCPSocket>&& socket);

          ~User();

          virtual
          void
          _handle() override;

          ELLE_ATTRIBUTE_R(bool, authentified);
          ELLE_ATTRIBUTE(meta::Admin, meta)
          ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_id);
          ELLE_ATTRIBUTE(std::string, user_id);
          ELLE_ATTRIBUTE(std::string, session_id);

        private:
          void
          _connect();

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
          void
          print(std::ostream& stream) const override;
        };
      }
    }
  }
}

#endif
