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
      struct AddSwaggerResponse: Response
      {
      };

      class Admin: public Client
      {
      public:
        Admin(std::string const& protocol,
              std::string const& host,
              uint16_t port);

        void
        connect(boost::uuids::uuid const& uuid,
                std::string const& user_id,
                boost::uuids::uuid const& device_uuid,
                elle::Version const& version,
                std::string const& os);

        Response
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
          boost::optional<std::string> zone = boost::optional<std::string>());

        Response
        unregister_trophonius(boost::uuids::uuid const& uuid);

        // Make it generic.
        Response
        register_apertus(boost::uuids::uuid const& uid,
                         std::string const& host,
                         uint16_t port_ssl,
                         uint16_t port_tcp);

        Response
        unregister_apertus(boost::uuids::uuid const& uid);

        Response
        apertus_update_bandwidth(boost::uuids::uuid const& uid,
                                 uint32_t bandwidth,
                                 uint32_t number_of_transfers);

        AddSwaggerResponse
        add_swaggers(std::string const& user1, std::string const& user2) const;

        Response
        ghostify(std::string const& email) const;

        Response
        genocide() const;
      };
    }
  }
}

#endif
