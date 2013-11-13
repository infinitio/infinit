#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_TROPHONIUS_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_TROPHONIUS_HH

# include <unordered_set>

# include <boost/date_time/posix_time/posix_time.hpp>
# include <boost/multi_index_container.hpp>
# include <boost/multi_index/composite_key.hpp>
# include <boost/multi_index/mem_fun.hpp>
# include <boost/multi_index/identity.hpp>
# include <boost/multi_index/hashed_index.hpp>
# include <boost/uuid/uuid.hpp>

# include <elle/attribute.hh>

# include <reactor/Barrier.hh>
# include <reactor/network/tcp-server.hh>
# include <reactor/rw-mutex.hh>
# include <reactor/thread.hh>
# include <reactor/waitable.hh>

# include <infinit/oracles/trophonius/server/fwd.hh>
# include <infinit/oracles/trophonius/server/User.hh>
# include <infinit/oracles/meta/Admin.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        namespace bmi = boost::multi_index;

        class UnknownClient:
          public elle::Exception
        {
        public:
          UnknownClient(std::string const& user_id,
                        boost::uuids::uuid const& device_id);
        };

        class Trophonius:
          public reactor::Waitable
        {
        public:
          Trophonius(int port,
                     std::string const& meta_host,
                     int meta_port,
                     int notifications_port = 0,
                     boost::posix_time::time_duration const& user_ping_period = 30_sec,
                     boost::posix_time::time_duration const& ping_period = 5_sec);

          ~Trophonius();
          void
          stop();

        /*-------.
        | Server |
        `-------*/
        private:
          void
          _serve();

          void
          _serve_notifier();

          ELLE_ATTRIBUTE(reactor::network::TCPServer, server);
          ELLE_ATTRIBUTE_r(int, port);
          ELLE_ATTRIBUTE(reactor::network::TCPServer, notifications);
          ELLE_ATTRIBUTE(reactor::Barrier, ready);

          int
          notification_port() const;
          ELLE_ATTRIBUTE(reactor::Thread, accepter);
          ELLE_ATTRIBUTE(reactor::Thread, meta_accepter);
          ELLE_ATTRIBUTE_R(boost::uuids::uuid, uuid);
          ELLE_ATTRIBUTE_R(meta::Admin, meta);
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, meta_pinger);

        /*--------.
        | Clients |
        `--------*/
        public:
          void
          client_remove(Client& c);
          User&
          user(std::string const& user_id,
               boost::uuids::uuid const& device);
          // struct hash_user_device
          //   : public std::unary_function<User*, std::size_t>
          // {
          //   std::size_t operator()(User* user) const;
          // };
          typedef boost::multi_index_container<
            User*,
            bmi::indexed_by<
              bmi::hashed_unique<bmi::identity<User*> >,
              bmi::hashed_unique<
                bmi::composite_key<
                  User*,
                  bmi::const_mem_fun<User, std::string const&, &User::user_id>,
                  bmi::const_mem_fun<User, boost::uuids::uuid const&, &User::device_id>
                >
              >
            >
          > Users;
          // typedef std::unordered_set<User*> Users;
          ELLE_ATTRIBUTE_RX(Users, users);
          typedef std::unordered_set<User*> PendingUsers;
          ELLE_ATTRIBUTE_RX(PendingUsers, users_pending);
          typedef std::unordered_set<Meta*> Metas;
          ELLE_ATTRIBUTE(Metas, metas);
          ELLE_ATTRIBUTE_R(boost::posix_time::time_duration, ping_period);
          ELLE_ATTRIBUTE(reactor::RWMutex, remove_lock);

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
