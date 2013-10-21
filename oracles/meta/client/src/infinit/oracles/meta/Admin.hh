#ifndef ADMIN_HH
# define ADMIN_HH

#include <infinit/oracles/meta/Client.hh>

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
        Admin(std::string const& admin_token,
              std::string const& host,
              uint16_t port);

        Response
        connect(std::string const& uid,
                std::string const& user_id,
                std::string const& device_id);

        Response
        disconnect(std::string const& uid,
                   std::string const& user_id,
                   std::string const& device_id);

        Response
        register_trophonius(std::string const& uid,
                            std::string const& ip,
                            uint16_t port);

        Response
        unregister_trophonius(std::string const& uid);

        AddSwaggerResponse
        add_swaggers(std::string const& user1, std::string const& user2) const;

        Response
        ghostify(std::string const& email) const;

        Response
        genocide() const;

        ELLE_ATTRIBUTE_RW(std::string, token);
      };
    }
  }
}

#endif
