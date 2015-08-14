#include <elle/log.hh>
#include <elle/test.hh>

#include <reactor/network/exception.hh>

#include <infinit/oracles/trophonius/Client.hh>

#include <surface/gap/Error.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("fist.tests")

/*-------------.
| Normal Login |
`-------------*/

ELLE_TEST_SCHEDULED(normal_login)
{
  tests::Server server;
  auto const& user = server.register_user("bob@infinit.io", "password");
  elle::filesystem::TemporaryDirectory user_home("normal_login");
  tests::Client client(server, user, user_home.path());
  BOOST_CHECK_NO_THROW(client.login());
}

/*--------------------.
| Meta Login Failures |
`--------------------*/

namespace tests
{
  class LoginFailServer
    : public Server
  {
  public:
    ELLE_ATTRIBUTE_RW(infinit::oracles::meta::Error, throw_code);

  protected:
    virtual
    std::string
    _login_post(Headers const& headers,
                Cookies const& cookies,
                Parameters const& parameters,
                elle::Buffer const& body) override
    {
      std::stringstream res;
      {
        elle::serialization::json::SerializerOut output(res, false);
        output.serialize("code", static_cast<int>(this->throw_code()));
      }
      throw Server::Exception(
        "/login",
        reactor::http::StatusCode::Forbidden,
        res.str());
    }
  };
}

ELLE_TEST_SCHEDULED(meta_login_failure)
{
  tests::LoginFailServer server;
  auto const& user = server.register_user("bob@infinit.io", "password");
  elle::filesystem::TemporaryDirectory user_home("meta_login_failure");
  tests::Client client(server, user, user_home.path());
  using MetaError = infinit::oracles::meta::Error;
  namespace state_ns = infinit::state;
  // Email not confirmed.
  server.throw_code(MetaError::email_not_confirmed);
  BOOST_CHECK_THROW(client.state->login("bob@infinit.io", "password"),
                    state_ns::UnconfirmedEmailError);
  // Incorrect email/password.
  server.throw_code(MetaError::email_password_dont_match);
  BOOST_CHECK_THROW(client.state->login("wrong@infinit.io", "password"),
                    state_ns::CredentialError);
  // Already logged in.
  server.throw_code(MetaError::already_logged_in);
  BOOST_CHECK_THROW(client.state->login("bob@infinit.io", "password"),
                    state_ns::AlreadyLoggedIn);
  // Version rejected.
  server.throw_code(MetaError::deprecated);
  BOOST_CHECK_THROW(client.state->login("bob@infinit.io", "password"),
                    state_ns::VersionRejected);
  // Email not valid.
  server.throw_code(MetaError::email_not_valid);
  BOOST_CHECK_THROW(client.state->login("invalid_email", "password"),
                    state_ns::MissingEmail);
}

/*--------------------------.
| Trophonius Login Failures |
`--------------------------*/

namespace tests
{
  class ForbiddenTrophonius
    : public tests::Trophonius
  {
  public:
    ForbiddenTrophonius()
      : Trophonius()
      , _i(0)
    {}

  protected:
    virtual
    void
    _serve(std::unique_ptr<reactor::network::SSLSocket> socket) override
    {
      if (++this->_i == 2)
        return;
      tests::Trophonius::_serve(std::move(socket));
    }

    ELLE_ATTRIBUTE(int, i);
  };

  class TrophoniusForbiddenMeta
    : public tests::Server
  {
  public:
    TrophoniusForbiddenMeta(std::unique_ptr<ForbiddenTrophonius> trophonius)
      : tests::Server(std::move(trophonius))
    {}

  protected:
    virtual
    std::string
    _get_trophonius(Headers const& headers,
                    Cookies const& cookies,
                    Parameters const& parameters,
                    elle::Buffer const& body) const override
    {
      if (cookies.at("session-id") !=
          boost::lexical_cast<std::string>(this->session_id()))
        throw Exception(
          "/trophonius",
          reactor::http::StatusCode::Forbidden,
          "{}");
      return tests::Server::_get_trophonius(headers, cookies, parameters, body);
    }
  };
}

ELLE_TEST_SCHEDULED(trophonius_forbidden)
{
  auto tropho_server = elle::make_unique<tests::ForbiddenTrophonius>();
  tests::TrophoniusForbiddenMeta server(std::move(tropho_server));
  auto const& user = server.register_user("bob@infinit.io", "password");
  elle::filesystem::TemporaryDirectory user_home("trophonius_forbidden");
  tests::Client client(server, user, user_home.path());
  namespace tropho_ns = infinit::oracles::trophonius;
  reactor::Signal reconnected;
  auto tropho_client = elle::make_unique<tropho_ns::Client>(
    [&] (infinit::oracles::trophonius::ConnectionState connected)
    {
      ELLE_LOG("Received update: %s", connected.connected);
      if (!connected.connected)
        server.session_id(elle::UUID::random());
      else
        reconnected.signal();
    },
    std::bind(&surface::gap::State::on_reconnection_failed,
              &client.state.state()),
    client.state.trophonius_fingerprint());
  tropho_client->ping_period(500_ms);
  tropho_client->reconnection_cooldown(0_ms);
  ELLE_LOG("Logging in...");
  client.state->login("bob@infinit.io", "password", std::move(tropho_client));
  ELLE_LOG("Logged in, waiting for reconnected");
  reactor::wait(reconnected);
}

namespace tests
{
  class TimeoutTrophonius
    : public tests::Trophonius
  {
  protected:
    virtual
    void
    _serve(std::unique_ptr<reactor::network::SSLSocket> socket) override
    {
      {
        elle::serialization::json::SerializerOut output(*socket, false);
        output.serialize("poke", std::string("ouch"));
      }
      reactor::sleep(1_sec);
      {
        elle::serialization::json::SerializerOut output(*socket, false);
        output.serialize("notification_type", -666);
        output.serialize("response_code", 200);
        output.serialize("response_details", std::string("details"));
      }
      auto connect_data = elle::json::read(*socket);
      try
      {
        while (true)
          socket->read_until("\n");
      }
      catch (reactor::network::ConnectionClosed const&)
      {}
    }
  };
}

ELLE_TEST_SCHEDULED(trophonius_timeout)
{
  auto tropho_server = elle::make_unique<tests::TimeoutTrophonius>();
  tests::Server server(std::move(tropho_server));
  auto const& user = server.register_user("bob@infinit.io", "password");
  elle::filesystem::TemporaryDirectory user_home("trophonius_timeout");
  tests::Client client(server, user, user_home.path());
  namespace tropho_ns = infinit::oracles::trophonius;
  auto tropho_client = elle::make_unique<tropho_ns::Client>(
    [&] (tropho_ns::ConnectionState connected) {},
    std::bind(&surface::gap::State::on_reconnection_failed,
              &client.state.state()),
    client.state.trophonius_fingerprint(),
    500_ms);
  auto tropho_client_ptr = tropho_client.get();
  client.state->reconnection_cooldown(100_ms);
  tropho_client->connect_timeout(100_ms);
  reactor::Thread thread(
    "fixed_login", [&]
    {
      reactor::sleep(2_sec);
      tropho_client_ptr->connect_timeout(1_min);
    });
  BOOST_CHECK_NO_THROW(client.state->login("bob@infinit.io",
                                           "password",
                                           std::move(tropho_client)));
  BOOST_CHECK_EQUAL(client.state->logged_in(), true);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(normal_login), 0, timeout);
  suite.add(BOOST_TEST_CASE(meta_login_failure), 0, timeout);
  suite.add(BOOST_TEST_CASE(trophonius_forbidden), 0, timeout);
  suite.add(BOOST_TEST_CASE(trophonius_timeout), 0, timeout);
}
