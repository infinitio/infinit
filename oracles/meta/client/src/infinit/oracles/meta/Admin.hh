#ifndef ADMIN_HH
# define ADMIN_HH

#include <infinit/oracles/meta/Client.hh>
#include <boost/uuid/uuid.hpp>

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
        Admin(std::string const& host,
              uint16_t port);

        Response
        connect(boost::uuids::uuid const& uuid,
                std::string const& user_id,
                boost::uuids::uuid const& device_uuid);

        Response
        disconnect(boost::uuids::uuid const& uuid,
                   std::string const& user_id,
                   boost::uuids::uuid const& device_uuid);

        Response
        register_trophonius(boost::uuids::uuid const& uuid,
                            uint16_t port);

        Response
        unregister_trophonius(boost::uuids::uuid const& uuid);

        // Make it generic.
        Response
        register_apertus(boost::uuids::uuid const& uid,
                         uint16_t port);

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
