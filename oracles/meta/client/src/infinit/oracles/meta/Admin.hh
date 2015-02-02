#ifndef ADMIN_HH
# define ADMIN_HH

# include <boost/uuid/uuid.hpp>

# include <elle/Version.hh>

# include <infinit/oracles/meta/Client.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      class Admin: public Client
      {
      public:
        Admin(std::string const& protocol,
              std::string const& host,
              uint16_t port);
        Admin(std::string const& meta);

        void
        connect(boost::uuids::uuid const& uuid,
                std::string const& user_id,
                boost::uuids::uuid const& device_uuid,
                elle::Version const& version,
                std::string const& os);

        void
        disconnect(boost::uuids::uuid const& uuid,
                   std::string const& user_id,
                   boost::uuids::uuid const& device_uuid);

        void
        register_trophonius(
          boost::uuids::uuid const& uuid,
          int port_notifications,
          int port_client,
          int port_client_ssl,
          std::string const& hostname,
          int users,
          bool shutting_down = false,
          boost::optional<std::string> zone = boost::optional<std::string>());

        void
        unregister_trophonius(boost::uuids::uuid const& uuid);

        // Make it generic.
        void
        register_apertus(boost::uuids::uuid const& uid,
                         std::string const& host,
                         uint16_t port_ssl,
                         uint16_t port_tcp);

        void
        unregister_apertus(boost::uuids::uuid const& uid);

        void
        apertus_update_bandwidth(boost::uuids::uuid const& uid,
                                 uint32_t bandwidth,
                                 uint32_t number_of_transfers);

        void
        add_swaggers(std::string const& user1, std::string const& user2) const;

        void
        ghostify(std::string const& email) const;

        void
        genocide() const;
      };
    }
  }
}

#endif
