#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

ELLE_TEST_SCHEDULED(normal)
{
  tests::Server server;
  auto connect = [] (tests::State& state)
  {
    state.attach_callback<surface::gap::State::ConnectionStatus>(
      [&] (surface::gap::State::ConnectionStatus const& notif)
      {
        ELLE_TRACE_SCOPE("connection status notification: %s", notif);
      });
    state.attach_callback<surface::gap::State::UserStatusNotification>(
      [&] (surface::gap::State::UserStatusNotification const& notif)
      {
        ELLE_TRACE_SCOPE("user status notification: %s", notif);
      });
  };
  tests::State recipient(server, random_uuid());
  connect(recipient);
  ELLE_LOG("foo");
  recipient.facebook_connect("code");
  reactor::wait(recipient.logged_in());
  auto recipient_id = recipient.me().id;
  ELLE_LOG("bar");
  recipient.logout();
  recipient.facebook_connect("code");
  reactor::wait(recipient.logged_in());

  ELLE_LOG("baz");
  ELLE_ASSERT(server.users().size() == 1);

  tests::State sender(server, random_uuid());
  connect(sender);
  sender.facebook_connect("code_2");
  reactor::wait(sender.logged_in());
  ELLE_ASSERT_NEQ(sender.passport(), recipient.passport());
  sender.logout();
  sender.facebook_connect("code_2");
  reactor::wait(sender.logged_in());
  ELLE_ASSERT_NEQ(sender.passport(), recipient.passport());
  ELLE_ASSERT(server.users().size() == 2);
  ELLE_ASSERT_NEQ(sender.me().id, recipient_id);
}


ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 20;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(normal), 0, timeout);
}
