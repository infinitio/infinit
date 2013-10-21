#include <infinit/oracles/meta/Admin.hh>
#include <infinit/oracles/meta/macro.hh>

#include <elle/serialize/JSONArchive.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/MapSerializer.hxx>
#include <elle/serialize/SetSerializer.hxx>

#include <elle/printf.hh>

#include <cstdint>

SERIALIZE_RESPONSE(infinit::oracles::meta::Response, ar, res)
{
  (void) ar;
  (void) res;
}

SERIALIZE_RESPONSE(infinit::oracles::meta::AddSwaggerResponse, ar, res)
{
  (void) ar;
  (void) res;
}

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      Admin::Admin(std::string const& admin_token,
                   std::string const& host,
                   uint16_t port):
        Client(host, port),
        _token(admin_token)
      {}

      Response
      Admin::connect(std::string const& uid,
                     std::string const& user_id,
                     std::string const& device_id)
      {
        // Change GET to PUT.
        return this->_get<Response>(
          elle::sprintf("/trophonius/%s/users/%s/%s",
                        uid, user_id, device_id));
      }

      Response
      Admin::disconnect(std::string const& uid,
                        std::string const& user_id,
                        std::string const& device_id)
      {
        // Change GET to DELETE.
        return this->_get<Response>(
          elle::sprintf("/trophonius/%s/users/%s/%s",
                        uid, user_id, device_id));
      }

      Response
      Admin::register_trophonius(std::string const& uid,
                                 std::string const& ip,
                                 uint16_t port)
      {
        json::Dictionary request;
        request["ip"] = ip;
        request["port"] = port;

        // Change POST to PUT.
        return this->_post<Response>(elle::sprintf("/trophonius/%s", uid),
                                     request);
      }

      Response
      Admin::unregister_trophonius(std::string const& uid)
      {
        // Change GET to DELETE.
        return this->_get<Response>(elle::sprintf("/trophonius/%s", uid));
      }

      Response
      Admin::genocide() const
      {
        json::Dictionary request;
        request["admin_token"] = this->token();
        return this->_post<Response>("/genocide", request);
      }

      Response
      Admin::ghostify(std::string const& email) const
      {
        json::Dictionary request;
        request["admin_token"] = this->token();
        request["email"] = email;
        return this->_post<Response>("/ghostify", request);
      }

      AddSwaggerResponse
      Admin::add_swaggers(std::string const& user1, std::string const& user2) const
      {
        json::Dictionary request;
        request["user1"] = user1;
        request["user2"] = user2;
        request["admin_token"] = this->token();
        return this->_post<AddSwaggerResponse>("/user/add_swagger", request);
      }
    }
  }
}
