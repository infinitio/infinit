#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

ELLE_TEST_SCHEDULED(early_402)
{
  tests::Server server;
  server.register_route(
    "/link_empty",
    reactor::http::Method::POST,
    [&] (tests::Server::Headers const&,
         tests::Server::Cookies const&,
         tests::Server::Parameters const&,
         elle::Buffer const&)
    {
      throw reactor::http::tests::Server::Exception(
        "/link_empty",
        reactor::http::StatusCode::Payment_Required,
        "{"
        "  \"reason\":\"Quota exhausted\","
        "  \"quota\":100000,"
        "  \"usage\":30000"
        "}");
      return "{}";
    });
  reactor::Barrier finished, failed;
  elle::filesystem::TemporaryFile transfered("cloud-buffered");
  {
    boost::filesystem::ofstream f(transfered.path());
    BOOST_CHECK(f.good());
    for (int i = 0; i < 2048; ++i)
    {
      char c = i % 256;
      f.write(&c, 1);
    }
  }
  tests::Client sender(server, "sender@infinit.io");
  bool beacon = false;
  sender.state->attach_callback<surface::gap::LinkTransaction>(
    [&] (surface::gap::LinkTransaction const& transaction)
    {
      if (transaction.status == gap_transaction_new)
        beacon = true;
      if (transaction.status == gap_transaction_payment_required)
        finished.open();
      if (transaction.status == gap_transaction_failed)
        failed.open();
    });
  sender.login();
  sender.state->create_link(
    std::vector<std::string>{transfered.path().string().c_str()}, "message");

  while (!finished && !failed)
  {
    reactor::sleep(100_ms);
    sender.state->poll();
  }
  BOOST_CHECK_EQUAL(beacon, true);
}

ELLE_TEST_SCHEDULED(other_402)
{
  tests::Server server;
  server.register_route(
    "/link_empty",
    reactor::http::Method::POST,
    [&] (tests::Server::Headers const&,
         tests::Server::Cookies const&,
         tests::Server::Parameters const&,
         elle::Buffer const&)
    {
      return "{\"created_link_id\": \"de305d54-75b4-431b-adb2-eb6b9e546014\"}";
    });

  server.register_route(
    "/link/de305d54-75b4-431b-adb2-eb6b9e546014",
    reactor::http::Method::PUT,
    [&] (tests::Server::Headers const&,
         tests::Server::Cookies const&,
         tests::Server::Parameters const&,
         elle::Buffer const&)
    {
      throw reactor::http::tests::Server::Exception(
        "/link/de305d54-75b4-431b-adb2-eb6b9e546014",
        reactor::http::StatusCode::Payment_Required,
        "{"
        "  \"reason\":\"Quota exhausted\","
        "  \"quota\":100000,"
        "  \"usage\":30000"
        "}");
      return "{}";
    });

  reactor::Barrier finished, failed;
  elle::filesystem::TemporaryFile transfered("cloud-buffered");
  {
    boost::filesystem::ofstream f(transfered.path());
    BOOST_CHECK(f.good());
    for (int i = 0; i < 2048; ++i)
    {
      char c = i % 256;
      f.write(&c, 1);
    }
  }
  tests::Client sender(server, "sender@infinit.io");
  bool beacon = false;
  sender.state->attach_callback<surface::gap::LinkTransaction>(
    [&] (surface::gap::LinkTransaction const& transaction)
    {
      if (transaction.status == gap_transaction_new)
        beacon = true;
      if (transaction.status == gap_transaction_payment_required)
        finished.open();
      if (transaction.status == gap_transaction_failed)
        failed.open();
    });
  sender.login();
  sender.state->create_link(
    std::vector<std::string>{transfered.path().string().c_str()}, "message");

  while (!finished && !failed)
  {
    reactor::sleep(100_ms);
    sender.state->poll();
  }
  BOOST_CHECK_EQUAL(beacon, true);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(5);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(early_402), 0, timeout);
  suite.add(BOOST_TEST_CASE(other_402), 0, timeout);
}
