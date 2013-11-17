#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_USER_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_USER_HH

# include <queue>

# include <json_spirit/value.h>

# include <reactor/Barrier.hh>
# include <reactor/signal.hh>

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
        /*------.
        | Types |
        `------*/
        public:
          typedef Client Super;

        /*-------------.
        | Construction |
        `-------------*/
        public:
          User(Trophonius& trophonius,
               std::unique_ptr<reactor::network::TCPSocket>&& socket);
          ~User();
        protected:
          virtual
          void
          _terminate() override;

        /*--------.
        | Session |
        `--------*/
        public:
          ELLE_ATTRIBUTE_R(boost::uuids::uuid, device_id);
          ELLE_ATTRIBUTE_R(std::string, user_id);
          ELLE_ATTRIBUTE(std::string, session_id);

        /*-----.
        | Meta |
        `-----*/
        protected:
          virtual
          void
          _unregister() override;
        private:
          ELLE_ATTRIBUTE(meta::Admin, meta);
          ELLE_ATTRIBUTE_RX(reactor::Barrier, authentified);
          ELLE_ATTRIBUTE_R(bool, registered);

        /*--------------.
        | Notifications |
        `--------------*/
        public:
          void
          notify(json_spirit::Value const& notification);
        private:
          void
          _handle_notifications();
          ELLE_ATTRIBUTE(std::queue<json_spirit::Value>, notifications);
          ELLE_ATTRIBUTE(reactor::Signal, notification_available);
          ELLE_ATTRIBUTE(reactor::Thread, notifications_thread);

        /*-------.
        | Server |
        `-------*/
        protected:
          virtual
          void
          _handle() override;

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
