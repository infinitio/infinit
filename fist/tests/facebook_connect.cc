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
    state->attach_callback<surface::gap::State::ConnectionStatus>(
      [&] (surface::gap::State::ConnectionStatus const& notif)
      {
        ELLE_TRACE_SCOPE("connection status notification: %s", notif);
      });
    state->attach_callback<surface::gap::State::UserStatusNotification>(
      [&] (surface::gap::State::UserStatusNotification const& notif)
      {
        ELLE_TRACE_SCOPE("user status notification: %s", notif);
      });
  };
  tests::State recipient(server, elle::UUID::random());
  connect(recipient);
  recipient->facebook_connect("code");
  reactor::wait(recipient->logged_in());
  auto recipient_id = recipient->me().id;
  recipient->logout();
  recipient->facebook_connect("code");
  reactor::wait(recipient->logged_in());
  ELLE_ASSERT_EQ(server.users().size(), 1u);

  tests::State sender(server, elle::UUID::random());
  connect(sender);
  sender->facebook_connect("code_2");
  reactor::wait(sender->logged_in());
  ELLE_ASSERT_NEQ(sender->passport(), recipient->passport());
  sender->logout();
  sender->facebook_connect("code_2");
  reactor::wait(sender->logged_in());
  ELLE_ASSERT_NEQ(sender->passport(), recipient->passport());
  ELLE_ASSERT_EQ(server.users().size(), 2u);
  ELLE_ASSERT_NEQ(sender->me().id, recipient_id);
}


ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 20;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(normal), 0, timeout);
}
