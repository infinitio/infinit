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
# include <reactor/network/ssl-server.hh>
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
          Trophonius(
            int port_ssl,
            int port_tcp,
            std::string const& meta_protocol,
            std::string const& meta_host,
            int meta_port,
            int notifications_port = 0,
            boost::posix_time::time_duration const& user_ping_period = 30_sec,
            boost::posix_time::time_duration const& meta_ping_period = 60_sec,
            boost::posix_time::time_duration const& user_auth_max_time = 10_sec,
            bool meta_fatal = true,
            boost::optional<std::string> zone = boost::optional<std::string>());
          ~Trophonius();
          void
          stop();
          void
          terminate();

        /*-------.
        | Server |
        `-------*/
        private:
          void
          _serve(reactor::network::Server& server);
          void
          _serve_notifier();
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::SSLCertificate>,
                         certificate);
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::SSLServer>,
                         server_ssl);
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::network::TCPServer>,
                         server_tcp);
          ELLE_ATTRIBUTE_r(int, port_ssl);
          ELLE_ATTRIBUTE_r(int, port_tcp);
          ELLE_ATTRIBUTE(reactor::network::TCPServer, notifications);
          ELLE_ATTRIBUTE(reactor::Barrier, ready);
          int
          notification_port() const;
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter_ssl);
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter_tcp);
          ELLE_ATTRIBUTE(bool, meta_fatal);
          ELLE_ATTRIBUTE(reactor::Thread, meta_accepter);
          ELLE_ATTRIBUTE_R(boost::uuids::uuid, uuid);
          ELLE_ATTRIBUTE_R(meta::Admin, meta);
          ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, meta_pinger);
          ELLE_ATTRIBUTE(bool, terminating);

        /*--------.
        | Clients |
        `--------*/
        public:
          void
          client_remove(Client& c);
          User&
          user(std::string const& user_id,
               boost::uuids::uuid const& device);
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
          ELLE_ATTRIBUTE_RX(Users, users);
          typedef std::unordered_set<User*> PendingUsers;
          ELLE_ATTRIBUTE_RX(PendingUsers, users_pending);
          typedef std::unordered_set<Meta*> Metas;
          ELLE_ATTRIBUTE(Metas, metas);
          ELLE_ATTRIBUTE_R(boost::posix_time::time_duration, ping_period);
          ELLE_ATTRIBUTE_R(boost::posix_time::time_duration, user_auth_max_time);
          ELLE_ATTRIBUTE(reactor::RWMutex, remove_lock);
          ELLE_ATTRIBUTE_R(boost::optional<std::string>, zone);

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
