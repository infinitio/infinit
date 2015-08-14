#include <elle/log.hh>
#include <elle/test.hh>

#include <fist/tests/server.hh>

ELLE_LOG_COMPONENT("surface.gap.state.test");

struct AccountNotification
{
  AccountNotification(infinit::oracles::meta::Account const& account)
    : account(account)
  {}

public:
  void
  serialize(elle::serialization::Serializer& s)
  {
    uint32_t type = 42;
    s.serialize("notification_type", type);
    s.serialize("account", account);
  }

public:
  Account account;
};

namespace tests
{
  static
  void
  update_plan(tests::Server& server,
              tests::Client& client,
              infinit::oracles::meta::AccountPlanType new_plan,
              infinit::oracles::meta::AccountPlanType current_plan =
                infinit::oracles::meta::AccountPlanType_Basic)
  {
    BOOST_CHECK_EQUAL(client.state->model().account.plan,
                      current_plan);
    reactor::Barrier barrier;
    auto connection = client.state->model().account.plan.changed().connect(
      [&] (infinit::oracles::meta::AccountPlanType)
      {
        barrier.open();
      });

    infinit::oracles::meta::Account account;
    account.plan = new_plan;
    AccountNotification notif(account);
    std::stringstream ss;
    {
      elle::serialization::json::SerializerOut output(ss, false);
      notif.serialize(output);
    }

    server.notify_user(client.user.id, ss.str());
    barrier.wait();
    client.state->model().account.plan.changed().disconnect(connection);
    BOOST_CHECK_EQUAL(client.state->model().account.plan,
                      new_plan);
  }
}

ELLE_TEST_SCHEDULED(plan)
{
  tests::Server server;
  elle::filesystem::TemporaryDirectory home("model-update");
  tests::Client client(
    server,
    server.register_user("user@infinit.io", "password"),
    home.path());
  client.login();

  tests::update_plan(server, client,
                     infinit::oracles::meta::AccountPlanType_Plus,
                     infinit::oracles::meta::AccountPlanType_Basic);
  tests::update_plan(server, client,
                     infinit::oracles::meta::AccountPlanType_Premium,
                     infinit::oracles::meta::AccountPlanType_Plus);
  tests::update_plan(server, client,
                     infinit::oracles::meta::AccountPlanType_Basic,
                     infinit::oracles::meta::AccountPlanType_Premium);
}

namespace tests
{
  static
  void
  update_account(tests::Server& server,
                 tests::Client& client,
                 infinit::oracles::meta::Account const& account)
  {
    reactor::Barrier barrier;
    auto connection = client.state->model().account.quotas.changed().connect(
      [&] (infinit::oracles::meta::Account::Quotas const& quotas)
      {
        barrier.open();
      });

    AccountNotification notif(account);
    std::stringstream ss;
    {
      elle::serialization::json::SerializerOut output(ss, false);
      notif.serialize(output);
    }
    server.notify_user(client.user.id, ss.str());
    barrier.wait();
    client.state->model().account.plan.changed().disconnect(connection);
  }
}

ELLE_TEST_SCHEDULED(account)
{
  tests::Server server;
  elle::filesystem::TemporaryDirectory home("model-update");
  tests::Client client(
    server,
    server.register_user("user@infinit.io", "password"),
    home.path());
  client.login();

#define UPDATE_FIELD(field)                                                    \
  [&] (uint64_t value)                                                          \
  {                                                                            \
    infinit::oracles::meta::Account acc;                                       \
    infinit::oracles::meta::Account::Quotas quotas;                            \
    quotas.field = value;                                                      \
    acc.quotas = quotas;                                                       \
    tests::update_account(server, client, acc);                                \
    BOOST_CHECK_EQUAL(client.state->model().account.quotas.value().field,      \
                      value);                                                  \
  }
  UPDATE_FIELD(links.quota)(100);
  UPDATE_FIELD(links.quota)(1000);
  UPDATE_FIELD(links.used)(100);
  UPDATE_FIELD(send_to_self.quota)(100);
  UPDATE_FIELD(send_to_self.used)(100);
  UPDATE_FIELD(send_to_self.used)(10000);
  UPDATE_FIELD(p2p.limit)(100);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(plan), 0, timeout);
  suite.add(BOOST_TEST_CASE(account), 0, timeout);
}
