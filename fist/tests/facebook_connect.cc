#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#define private public
#include <surface/gap/State.hh>
#undef private

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

struct Reporter
  : public infinit::metrics::Reporter
{
  Reporter(reactor::Barrier& register_beacon,
           reactor::Barrier& facebook_beacon,
           bool check_ghost_code = false)
    : infinit::metrics::Reporter("osef")
    , register_beacon(register_beacon)
    , facebook_beacon(facebook_beacon)
    , check_ghost_code(check_ghost_code)
  {}

  void
  _user_register(bool success,
                 std::string const& info,
                 std::string const& with,
                 boost::optional<std::string> ghost_code,
                 boost::optional<std::string> referral_code) override
  {
    ELLE_ASSERT_EQ(success, true);
    ELLE_ASSERT_EQ(with, "facebook");
    if (this->check_ghost_code)
      ELLE_ASSERT_EQ(static_cast<bool>(ghost_code), true);
    this->register_beacon.open();
  }

  void
  _facebook_connect(bool success,
                    std::string const& info) override
  {
    BOOST_CHECK_EQUAL(success, true);
    this->facebook_beacon.open();
  }

  reactor::Barrier& register_beacon;
  reactor::Barrier& facebook_beacon;
  bool check_ghost_code;
};

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
  auto check_metric = [] (reactor::Barrier& barrier)
  {
    reactor::wait(barrier);
    barrier.close();
  };
  reactor::Barrier registered;
  reactor::Barrier facebook_connect;
  auto install_reporter = [&] (surface::gap::State& state)
    {
      auto rep = elle::make_unique<infinit::metrics::CompositeReporter>();
      rep->add_reporter(
        elle::make_unique<Reporter>(registered, facebook_connect));
      state._metrics_reporter.reset(rep.release());
      state._metrics_reporter->start();
    };

  tests::State recipient(server, elle::UUID::random());
  connect(recipient);
  install_reporter(recipient.state());
  recipient->facebook_connect("code");
  reactor::wait(recipient->logged_in());
  check_metric(registered);
  check_metric(facebook_connect);
  auto recipient_id = recipient->me().id;
  recipient->logout();
  recipient->facebook_connect("code");
  reactor::wait(recipient->logged_in());
  check_metric(facebook_connect);
  ELLE_ASSERT_EQ(server.users().size(), 1u);

  tests::State sender(server, elle::UUID::random());
  connect(sender);
  install_reporter(sender.state());
   sender->facebook_connect("code_2");
  reactor::wait(sender->logged_in());
  check_metric(registered);
  check_metric(facebook_connect);
  ELLE_ASSERT_NEQ(sender->passport(), recipient->passport());
  sender->logout();
  sender->facebook_connect("code_2");
  reactor::wait(sender->logged_in());
  check_metric(facebook_connect);
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
