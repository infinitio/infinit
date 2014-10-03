#include <infinit/oracles/meta/Admin.hh>
#include <infinit/oracles/meta/macro.hh>

#include <elle/serialize/JSONArchive.hh>
#include <elle/format/json/Dictionary.hh>
#include <elle/serialization/json.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/MapSerializer.hxx>
#include <elle/serialize/SetSerializer.hxx>

#include <elle/printf.hh>

#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>

#include <version.hh>

#include <cstdint>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      using elle::sprintf;
      using reactor::http::Method;

      Admin::Admin(std::string const& protocol,
                   std::string const& host,
                   uint16_t port):
        Client(protocol, host, port)
      {}

      void
      Admin::connect(boost::uuids::uuid const& uid,
                     std::string const& user_id,
                     boost::uuids::uuid const& device_id,
                     elle::Version const& version)
      {
        auto url = elle::sprintf("/trophonius/%s/users/%s/%s",
                                 boost::lexical_cast<std::string>(uid),
                                 user_id,
                                 boost::lexical_cast<std::string>(device_id));
        auto request = this->_request(
          url, Method::PUT,
          [&version] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request);
            output.serialize("version", const_cast<elle::Version&>(version));
          });
        elle::serialization::json::SerializerIn input(request);
      }

      void
      Admin::disconnect(boost::uuids::uuid const& uid,
                        std::string const& user_id,
                        boost::uuids::uuid const& device_id)
      {
        std::string const url =
          elle::sprintf("/trophonius/%s/users/%s/%s", uid, user_id, device_id);
        this->_request(url, Method::DELETE);
      }

      void
      Admin::register_trophonius(
        boost::uuids::uuid const& uid,
        int port,
        int port_client,
        int port_client_ssl,
        std::string const& hostname,
        int users,
        boost::optional<std::string> zone)
      {
        auto url = elle::sprintf("/trophonius/%s",
                                 boost::lexical_cast<std::string>(uid));
        auto request = this->_request(
          url, Method::PUT,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request);
            output.serialize("port", port);
            output.serialize("port_client", port_client);
            output.serialize("port_client_ssl", port_client_ssl);
            output.serialize("hostname", const_cast<std::string&>(hostname));
            output.serialize("users", users);
            std::string version = INFINIT_VERSION;
            output.serialize("version", version);
            if (zone)
              output.serialize("zone", zone.get());
          });
        elle::serialization::json::SerializerIn input(request);
      }

      void
      Admin::unregister_trophonius(boost::uuids::uuid const& uid)
      {
        this->_request(sprintf("/trophonius/%s", uid), Method::DELETE);
      }

      void
      Admin::register_apertus(boost::uuids::uuid const& uid,
                              std::string const& host,
                              uint16_t port_ssl,
                              uint16_t port_tcp)
      {
        std::string const url = sprintf("/apertus/%s", uid);
        this->_request(
          url,
          Method::PUT,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request);
            output.serialize("host", const_cast<std::string&>(host));
            int port_ssl_ = port_ssl;
            output.serialize("port_ssl", port_ssl_);
            int port_tcp_ = port_tcp;
            output.serialize("port_tcp", port_tcp_);
            std::string version = INFINIT_VERSION;
            output.serialize("version", version);
          });
      }

      void
      Admin::unregister_apertus(boost::uuids::uuid const& uid)
      {
        std::string const url = sprintf("/apertus/%s", uid);
        this->_request(url, Method::DELETE);
      }

      void
      Admin::apertus_update_bandwidth(boost::uuids::uuid const& uid,
                                      uint32_t bandwidth,
                                      uint32_t number_of_transfers)
      {
        std::string const url = sprintf("/apertus/%s/bandwidth", uid);
        this->_request(
          url,
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request);
            int bandwidth_ = bandwidth;
            output.serialize("bandwidth", bandwidth_);
            int number = number_of_transfers;
            output.serialize("number_of_transfers", number);
          });
      }

      void
      Admin::genocide() const
      {
        this->_request("/genocide", Method::DELETE);
      }

      void
      Admin::ghostify(std::string const& email) const
      {
        this->_request(
          "/ghostify",
          Method::POST,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request);
            output.serialize("email", const_cast<std::string&>(email));
          });
      }

      void
      Admin::add_swaggers(std::string const& user1,
                          std::string const& user2) const
      {
        this->_request(
          "/user/add_swagger",
          Method::PUT,
          [&] (reactor::http::Request& request)
          {
            elle::serialization::json::SerializerOut output(request);
            output.serialize("user1", const_cast<std::string&>(user1));
            output.serialize("user2", const_cast<std::string&>(user2));
          });
      }
    }
  }
}
