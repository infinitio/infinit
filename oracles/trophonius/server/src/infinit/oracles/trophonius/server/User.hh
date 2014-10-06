#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_USER_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_USER_HH

# include <queue>

# include <elle/Version.hh>
# include <elle/json/json.hh>

# include <reactor/Barrier.hh>
# include <reactor/Channel.hh>
# include <reactor/network/socket.hh>
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
               std::unique_ptr<reactor::network::Socket>&& socket);
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
          ELLE_ATTRIBUTE_R(elle::Version, version);
          ELLE_ATTRIBUTE(std::string, os);

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
          notify(elle::json::Object const& notification);
        private:
          void
          _handle_notifications();
          ELLE_ATTRIBUTE(reactor::Channel<elle::json::Object>, notifications);
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
