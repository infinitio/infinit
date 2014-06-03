#include <boost/uuid/nil_generator.hpp>

#include <elle/test.hh>
#include <elle/log.hh>
#include <elle/Exception.hh>
#include <elle/json/exceptions.hh>
#include <elle/serialization/json/MissingKey.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/meta/Client.hh>
#include <surface/gap/Exception.hh>
#include <surface/gap/Error.hh>

#include <http_server.hh>

ELLE_LOG_COMPONENT("bite");

typedef reactor::http::tests::Server HTTPServer;

ELLE_TEST_SCHEDULED(connection_refused)
{
  infinit::oracles::meta::Client c("http", "127.0.0.1", 21232);
}

ELLE_TEST_SCHEDULED(login_success)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     return "{"
                       " \"_id\": \"0\","
                       " \"fullname\": \"jean\","
                       " \"email\": \"jean@infinit.io\","
                       " \"handle\": \"jean\","
                       " \"register_status\": \"ok\","
                       " \"device_id\": \"device_id\","
                       " \"trophonius\": {\"host\": \"192.168.1.1\", \"port\": 4923, \"port_ssl\": 4233},"
                       " \"identity\": \"identity\""
                       " }";
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  c.login("jean@infinit.io", "password", boost::uuids::nil_uuid());
}

ELLE_TEST_SCHEDULED(forbidden)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     throw HTTPServer::Exception(
                       "", reactor::http::StatusCode::Forbidden);
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  // Find good error.
  BOOST_CHECK_THROW(c.login("jean@infinit.io",
                            "password",
                            boost::uuids::nil_uuid()),
                    elle::Exception);
}

ELLE_TEST_SCHEDULED(ill_formed_json)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     return "{\"a\a:}";
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  BOOST_CHECK_THROW(c.login("jean@infinit.io",
                            "password",
                            boost::uuids::nil_uuid()),
                    std::runtime_error);
}

ELLE_TEST_SCHEDULED(missing_key)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     return "{}";
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  BOOST_CHECK_THROW(c.login("jean@infinit.io",
                            "password",
                            boost::uuids::nil_uuid()),
                    elle::serialization::MissingKey);
}

ELLE_TEST_SCHEDULED(login_password_dont_match)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     ELLE_LOG("body: %s", body);
                     throw HTTPServer::Exception("",
                                                 reactor::http::StatusCode::Forbidden,
                                                 "{"
                                                 " \"code\": -10101,"
                                                 " \"message\": \"email password dont match\""
                                                 "}");
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  BOOST_CHECK_THROW(c.login("jean@infinit.io",
                            "password",
                            boost::uuids::nil_uuid()),
                      infinit::state::CredentialError);
}

ELLE_TEST_SCHEDULED(unconfirmed_email)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     ELLE_LOG("body: %s", body);
                     throw HTTPServer::Exception("",
                                                 reactor::http::StatusCode::Forbidden,
                                                 "{"
                                                 " \"code\": -105,"
                                                 " \"message\": \"email not confirmed\""
                                                 "}");
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  BOOST_CHECK_THROW(c.login("jean@infinit.io",
                            "password",
                            boost::uuids::nil_uuid()),
                      infinit::state::UnconfirmedEmailError);
}

ELLE_TEST_SCHEDULED(already_logged_in)
{
  HTTPServer s;
  s.register_route("/login", reactor::http::Method::POST,
                   [] (HTTPServer::Headers const&,
                       HTTPServer::Cookies const&,
                       elle::Buffer const& body) -> std::string
                   {
                     ELLE_LOG("body: %s", body);
                     throw HTTPServer::Exception("",
                                                 reactor::http::StatusCode::Forbidden,
                                                 "{"
                                                 " \"code\": -102,"
                                                 " \"message\": \"already logged in\""
                                                 "}");
                   });
  infinit::oracles::meta::Client c("http", "127.0.0.1", s.port());
  BOOST_CHECK_THROW(c.login("jean@infinit.io",
                            "password",
                            boost::uuids::nil_uuid()),
                      infinit::state::AlreadyLoggedIn);
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(connection_refused));
  suite.add(BOOST_TEST_CASE(forbidden));
  suite.add(BOOST_TEST_CASE(login_success));
  suite.add(BOOST_TEST_CASE(ill_formed_json));
  suite.add(BOOST_TEST_CASE(missing_key));
  suite.add(BOOST_TEST_CASE(login_password_dont_match));
  suite.add(BOOST_TEST_CASE(unconfirmed_email));
  suite.add(BOOST_TEST_CASE(already_logged_in));


}
