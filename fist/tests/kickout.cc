#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("infinit.fist.tests.kickout");

namespace tests
{
  class KickoutServer
    : public Server
  {
  public:
    KickoutServer()
      : Server()
    {
      this->register_route(
        "/transactions",
        reactor::http::Method::POST,
        [&] (Server::Headers const&,
             Server::Cookies const&,
             Server::Parameters const&,
             elle::Buffer const&)
        {
          throw Exception(
            "/transactions",
            reactor::http::StatusCode::Forbidden,
            "{}");
          return "";
        });

      this->register_route(
        "/debug/report/transaction",
        reactor::http::Method::POST,
        [&] (Server::Headers const&,
             Server::Cookies const&,
             Server::Parameters const&,
             elle::Buffer const&)
        {
          return "{}";
        });
    }
  };
}

ELLE_TEST_SCHEDULED(kickout)
{
  tests::KickoutServer server;
  bool beacon = false;
  {
    tests::Client sender(server, "sender@infinit.io");
    sender.login();
    elle::filesystem::TemporaryFile transfered("filename");
    {
      boost::filesystem::ofstream f(transfered.path());
      BOOST_CHECK(f.good());
      for (int i = 0; i < 2048; ++i)
      {
        char c = i % 256;
        f.write(&c, 1);
      }
    }
    sender.state->attach_callback<surface::gap::State::ConnectionStatus>(
      [&] (surface::gap::State::ConnectionStatus const& connection){
        BOOST_CHECK_EQUAL(connection.status, false);
        BOOST_CHECK_EQUAL(connection.still_trying, true);
        beacon = true;
      });
    sender.state->transaction_peer_create(
      "foo@bar.io",
      std::vector<std::string>{transfered.path().string().c_str()},
      "message");
    reactor::sleep(500_ms);
    sender.state->poll();
  }
  BOOST_CHECK_EQUAL(beacon, true);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(kickout), 0, timeout);
}
