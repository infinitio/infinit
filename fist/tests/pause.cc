#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.state.test");

ELLE_TEST_SCHEDULED(pause_transfer)
{
  tests::SleepyServer server;
  elle::filesystem::TemporaryDirectory sender_home(
    "pause_sender_home_p2p");
  auto const& sender_user =
    server.register_user("sender@infinit.io", "password");
  elle::filesystem::TemporaryDirectory recipient_home(
    "pause_recipient_home_p2p");
  auto const& recipient_user =
    server.register_user("recipient@infinit.io", "password");
  std::string t_id;
  elle::filesystem::TemporaryFile transfered("pause");
  {
    boost::filesystem::ofstream f(transfered.path());
    BOOST_CHECK(f.good());
    for (int i = 0; i < 2048; ++i)
    {
      char c = i % 256;
      f.write(&c, 1);
    }
  }


  tests::Client sender(server, sender_user, sender_home.path());
  sender.login();
  auto& state_transaction = sender.state->transaction_peer_create(
    recipient_user.email(),
    std::vector<std::string>{transfered.path().string().c_str()},
    "message");
  reactor::Barrier paused;
  auto conn = state_transaction.status_changed().connect(
    [&] (gap_TransactionStatus status)
    {
      t_id = state_transaction.data()->id;
      ELLE_LOG("new sender transaction status: %s", status);
      if (status == gap_transaction_paused)
        paused.open();
    });
  reactor::wait(server.started_blocking);

  tests::Client recipient(server, recipient_user, recipient_home.path());
  recipient.login();
  BOOST_CHECK_EQUAL(recipient.state->transactions().size(), 1);
  auto& state_transaction_recipient = *recipient.state->transactions().begin()->second;
  reactor::Barrier recipient_finished;
  sender.state->transaction_pause(state_transaction.id());
  state_transaction_recipient.accept();
  reactor::wait(paused);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(pause_transfer), 0, timeout);
}
