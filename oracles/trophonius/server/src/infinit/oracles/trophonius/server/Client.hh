#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_CLIENT_HH

# include <memory>

# include <boost/uuid/uuid.hpp>

# include <elle/attribute.hh>

# include <reactor/network/socket.hh>
# include <reactor/thread.hh>
# include <reactor/timer.hh>

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
                 std::unique_ptr<reactor::network::Socket>&& socket);
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
          std::unique_ptr<reactor::network::Socket> _socket;

          ELLE_ATTRIBUTE(reactor::Thread, handle_thread);

        protected:
          /// Set a timer that calls terminate() when duraption elapses. Cancels previous timers.
          void
          _set_timeout(reactor::Duration d);
          /// Cancel a previously set timeout.
          void
          _cancel_timeout();

          ELLE_ATTRIBUTE(std::unique_ptr<reactor::Timer>, timeout_timer);

        /*-----.
        | Ward |
        `-----*/
        public:
          class RemoveWard:
            public elle::Finally
          {
          protected:
            RemoveWard(Client& c);
            ~RemoveWard() noexcept(false);
            friend class elle::With<RemoveWard>;
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
