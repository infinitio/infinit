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
      using elle::sprintf;
      using reactor::http::Method;

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
        return this->_request<Response>(
          sprintf("/trophonius/%s/users/%s/%s", uid, user_id, device_id),
          Method::PUT);
      }

      Response
      Admin::disconnect(std::string const& uid,
                        std::string const& user_id,
                        std::string const& device_id)
      {
        return this->_request<Response>(
          sprintf("/trophonius/%s/users/%s/%s", uid, user_id, device_id),
          Method::DELETE);
      }

      Response
      Admin::register_trophonius(std::string const& uid,
                                 std::string const& ip,
                                 uint16_t port)
      {
        json::Dictionary request;
        request["ip"] = ip;
        request["port"] = port;

        return this->_request<Response>(
          sprintf("/trophonius/%s", uid), Method::PUT, request);
      }

      Response
      Admin::unregister_trophonius(std::string const& uid)
      {
        return this->_request<Response>(
          sprintf("/trophonius/%s", uid), Method::DELETE);
      }

      Response
      Admin::genocide() const
      {
        json::Dictionary request;
        request["admin_token"] = this->token();
        return this->_request<Response>("/genocide", Method::DELETE, request);
      }

      Response
      Admin::ghostify(std::string const& email) const
      {
        json::Dictionary request;
        request["admin_token"] = this->token();
        request["email"] = email;
        return this->_request<Response>("/ghostify", Method::POST, request);
      }

      AddSwaggerResponse
      Admin::add_swaggers(std::string const& user1,
                          std::string const& user2) const
      {
        json::Dictionary request;
        request["user1"] = user1;
        request["user2"] = user2;
        request["admin_token"] = this->token();
        return this->_request<AddSwaggerResponse>(
          "/user/add_swagger", Method::PUT, request);
      }
    }
  }
}
