#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_USER_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_USER_HH

# include <infinit/oracles/trophonius/server/Client.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
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

          ELLE_ATTRIBUTE_R(reactor::Barrier, authentified);
          ELLE_ATTRIBUTE(meta::Admin, meta);
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
