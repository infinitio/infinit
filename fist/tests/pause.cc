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
    [&] (gap_TransactionStatus status, boost::optional<gap_Status>)
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
  sender.state->transaction_pause(state_transaction.id());
  state_transaction_recipient.accept();
  reactor::wait(paused);
}

ELLE_TEST_SCHEDULED(pause_snapshot)
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


  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    auto& state_transaction = sender.state->transaction_peer_create(
      recipient_user.email(),
      std::vector<std::string>{transfered.path().string().c_str()},
      "message");
    reactor::wait(server.started_blocking);
    sender.state->transaction_pause(state_transaction.id());
    reactor::wait(state_transaction.paused());
  }
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    sender.state->synchronize();
    auto& state_transaction = *sender.state->transactions().begin()->second;
    BOOST_CHECK(state_transaction.paused());
  }
}

ELLE_TEST_SCHEDULED(pause_resume)
{
  tests::Server server;
  elle::filesystem::TemporaryDirectory sender_home("pause_sender_home_p2p");
  auto const& sender_user =
    server.register_user("sender@infinit.io", "password");
  elle::filesystem::TemporaryDirectory recipient_home(
    "pause_recipient_home_p2p");
  auto const& recipient_user =
    server.register_user("recipient@infinit.io", "password");
  elle::filesystem::TemporaryFile transfered("pause");
  {
    boost::filesystem::ofstream f(transfered.path());
    BOOST_CHECK(f.good());
    for (int i = 0; i < (5 * 1024 * 1024); ++i)
    {
      char c = i % 256;
      f.write(&c, 1);
    }
  }

  tests::Client sender(server, sender_user, sender_home.path());
  reactor::Barrier first_transfer, paused, transfer_again;
  bool second_time = false;
  sender.state->attach_callback<surface::gap::PeerTransaction>(
    [&] (surface::gap::PeerTransaction const& transaction)
    {
      ELLE_WARN("new sender transaction status: %s", transaction.status);
      if (transaction.status == gap_transaction_paused)
        paused.open();
      if (transaction.status == gap_transaction_connecting ||
          transaction.status == gap_transaction_transferring)
      {
        if (second_time)
        {
          transfer_again.open();
        }
        else
        {
          second_time = true;
          first_transfer.open();
        }
      }
    });
  sender.login();
  auto& state_transaction = sender.state->transaction_peer_create(
    recipient_user.email(),
    std::vector<std::string>{transfered.path().string()},
    "message");

  tests::Client recipient(server, recipient_user, recipient_home.path());
  recipient.login();
  BOOST_CHECK_EQUAL(recipient.state->transactions().size(), 1);
  auto& state_transaction_recipient =
    *recipient.state->transactions().begin()->second;
  state_transaction_recipient.accept();
  while (!first_transfer)
  {
    sender.state->poll(); // Open first_transfer_barrier
    reactor::sleep(10_ms);
  }
  reactor::wait(first_transfer);
  sender.state->transaction_pause(state_transaction.id());
  while (!paused)
  {
    sender.state->poll(); // Open pause barrier
    reactor::sleep(10_ms);
  }
  reactor::wait(paused);
  sender.state->transaction_pause(state_transaction.id(), false);
  while (!transfer_again)
  {
    sender.state->poll(); // Open transfer_again barrier
    reactor::sleep(10_ms);
  }
  reactor::wait(transfer_again);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(25);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(pause_transfer), 0, timeout);
  suite.add(BOOST_TEST_CASE(pause_snapshot), 0, timeout);
  suite.add(BOOST_TEST_CASE(pause_resume), 0, timeout);
}
