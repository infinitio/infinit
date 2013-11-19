#include <infinit/oracles/meta/Admin.hh>
#include <infinit/oracles/meta/macro.hh>

#include <elle/serialize/JSONArchive.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/MapSerializer.hxx>
#include <elle/serialize/SetSerializer.hxx>

#include <elle/printf.hh>

#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

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

      Admin::Admin(std::string const& host,
                   uint16_t port):
        Client(host, port)
      {}

      Response
      Admin::connect(boost::uuids::uuid const& uid,
                     std::string const& user_id,
                     boost::uuids::uuid const& device_id)
      {
        return this->_request<Response>(
          sprintf("/trophonius/%s/users/%s/%s",
                  boost::lexical_cast<std::string>(uid),
                  user_id,
                  boost::lexical_cast<std::string>(device_id)),
          Method::PUT);
      }

      Response
      Admin::disconnect(boost::uuids::uuid const& uid,
                        std::string const& user_id,
                        boost::uuids::uuid const& device_id)
      {
        return this->_request<Response>(
          sprintf("/trophonius/%s/users/%s/%s",
                  boost::lexical_cast<std::string>(uid),
                  user_id,
                  boost::lexical_cast<std::string>(device_id)),
          Method::DELETE);
      }

      Response
      Admin::register_trophonius(boost::uuids::uuid const& uid,
                                 uint16_t port)
      {
        json::Dictionary request;
        request["port"] = port;

        return this->_request<Response>(
          sprintf("/trophonius/%s", boost::lexical_cast<std::string>(uid)),
          Method::PUT, request);
      }

      Response
      Admin::unregister_trophonius(boost::uuids::uuid const& uid)
      {
        return this->_request<Response>(
          sprintf("/trophonius/%s", boost::lexical_cast<std::string>(uid)),
          Method::DELETE);
      }

      // Make it generic.
      Response
      Admin::register_apertus(boost::uuids::uuid const& uid,
                              uint16_t port)
      {
        json::Dictionary request;
        request["port"] = port;

        return this->_request<Response>(
          sprintf("/apertus/%s", boost::lexical_cast<std::string>(uid)),
          Method::PUT, request);
      }

      Response
      Admin::unregister_apertus(boost::uuids::uuid const& uid)
      {
        return this->_request<Response>(
          sprintf("/apertus/%s", boost::lexical_cast<std::string>(uid)),
          Method::DELETE);
      }

      Response
      Admin::update_bandwidth(boost::uuids::uuid const& uid,
                              uint32_t bandwidth,
                              uint32_t number_of_transfers)
      {
        json::Dictionary request;
        request["bandwidth"] = bandwidth;
        request["number_of_transfers"] = number_of_transfers;

        return this->_request<Response>(
          sprintf("/apertus/%s/bandwidth",
                  boost::lexical_cast<std::string>(uid)),
          Method::POST, request);
      }

      Response
      Admin::genocide() const
      {
        json::Dictionary request;
        return this->_request<Response>("/genocide", Method::DELETE, request);
      }

      Response
      Admin::ghostify(std::string const& email) const
      {
        json::Dictionary request;
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
        return this->_request<AddSwaggerResponse>(
          "/user/add_swagger", Method::PUT, request);
      }
    }
  }
}
