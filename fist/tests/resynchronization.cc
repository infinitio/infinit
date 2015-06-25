#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.state.test");

ELLE_TEST_SCHEDULED(links)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  sender.login();
  ELLE_ASSERT_EQ(sender.state->transactions().size(), 0u);
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
  sender.state->create_link(
    std::vector<std::string>{transfered.path().string()}, "message", false);
  // Because our trophonius is a brick, we will not receive the notification.
  ELLE_ASSERT_EQ(sender.state->transactions().size(), 1u);
  // At the stage, status is new.
  ELLE_ASSERT_EQ(sender.state->transactions().begin()->second->data()->status,
                 infinit::oracles::Transaction::Status::created);

  while (true)
  {
    // At this stage, status has been set locally.
    if (sender.state->transactions().begin()->second->data()->status == infinit::oracles::Transaction::Status::finished)
      break;
    reactor::yield();
  }
  ELLE_ASSERT_EQ(sender.user.links.begin()->second.status, oracles::Transaction::Status::finished);

  sender.user.links.begin()->second.status =  oracles::Transaction::Status::canceled;
  // Disconnect trophonius.
  sender.state->synchronize();
  // At this stage, state should have resynchronization
  ELLE_ASSERT_EQ(sender.state->transactions().begin()->second->data()->status,
                 oracles::Transaction::Status::canceled);
}

ELLE_TEST_SCHEDULED(links_another_device)
{
  tests::Server server;
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

  infinit::oracles::LinkTransaction t;
  t.click_count = 3;
  t.id = boost::lexical_cast<std::string>(elle::UUID::random());
  t.ctime = 2173213;
  t.sender_id = boost::lexical_cast<std::string>(sender.user.id());
  t.sender_device_id = boost::lexical_cast<std::string>(sender.device_id) + "other";
  t.status = infinit::oracles::Transaction::Status::initialized;
  sender.user.links[t.id] = t;

  // Disconnect trophonius.
  sender.state->synchronize();

  ELLE_ASSERT_EQ(sender.state->transactions().begin()->second->data()->status,
                 oracles::Transaction::Status::initialized);

  sender.user.links[t.id].status = infinit::oracles::Transaction::Status::finished;

  sender.state->synchronize();

  ELLE_ASSERT_EQ(sender.state->transactions().begin()->second->data()->status,
                 oracles::Transaction::Status::finished);
}

ELLE_TEST_SCHEDULED(swaggers)
{
  tests::Server server;
  tests::Client bob(server, "bob@infinit.io");
  tests::Client alice(server, "alice@infinit.io");
  tests::Client eve(server, "eve@infinit.io");

  alice.user.swaggers.insert(&bob.user);
  bob.user.swaggers.insert(&alice.user);

  alice.login();
  bob.login();
  ELLE_ASSERT_EQ(bob.state->swaggers().size(), 1u);

  eve.user.swaggers.insert(&bob.user);
  bob.user.swaggers.insert(&eve.user);

  bool step0 = true; // Alice was logged in before (no notification).
  bool step1 = false;
  bob.state->attach_callback<surface::gap::State::UserStatusNotification>([&] (surface::gap::State::UserStatusNotification notif)
  {
    step0 = false;
    auto it = bob.state->users().find(notif.id);
    if (it->second.id == boost::lexical_cast<std::string>(eve.user.id()))
    {
      step1 = notif.status;
    }
  });
  bob.state->synchronize();
  bob.state->poll();
  ELLE_ASSERT(step0); // Nothing should have changed.
  ELLE_ASSERT(!step1); // Eve is a swagger but not online.
  ELLE_ASSERT_EQ(bob.state->swaggers().size(), 2u);
  eve.login();
  bob.state->synchronize();
  bob.state->poll();
  ELLE_ASSERT(!step0); // It should be valid now.
  ELLE_ASSERT(step1); // It should be valid now.
  ELLE_ASSERT_EQ(bob.state->swaggers().size(), 2u);

  // Disclaimer: It's really hard to play with multi device on a single device
  // when playing with real states. I'll use some random device ids to check
  // behaviours.
  bob.user.connected_devices.clear();
  bob.user.connected_devices.insert(elle::UUID::random());
  bob.user.connected_devices.insert(elle::UUID::random());

  int step2 = 0;
  alice.state->attach_callback<surface::gap::State::UserStatusNotification>([&] (surface::gap::State::UserStatusNotification notif)
  {
    auto it = alice.state->users().find(notif.id);
    if (it->second.id == boost::lexical_cast<std::string>(bob.user.id()))
    {
      step2 += notif.status;
    }
  });
  alice.state->synchronize();
  alice.state->poll();
  ELLE_ASSERT_EQ(step2, 2);

  bob.user.connected_devices.clear();
  bob.logout();

  bool step3 = false;
  alice.state->attach_callback<surface::gap::State::UserStatusNotification>([&] (surface::gap::State::UserStatusNotification notif)
  {
    auto it = alice.state->users().find(notif.id);
    if (it->second.id == boost::lexical_cast<std::string>(bob.user.id()))
    {
      step3 = !notif.status;
    }
  });

  alice.state->synchronize();
  alice.state->poll();
  ELLE_ASSERT(step3);
}

ELLE_TEST_SCHEDULED(disconnect)
{
  tests::Server server;
  tests::Client sender(server, "sender@infinit.io");
  tests::Client recipient(server, "recipient@infinit.io");
  sender.login();
  recipient.login();
  recipient.state->disconnect();
  elle::filesystem::TemporaryFile transfered("cloud-buffered");
  auto& transaction = sender.state->transaction_peer_create(
      "recipient@infinit.io",
      std::vector<std::string>{transfered.path().string().c_str()},
      "message");
  reactor::Barrier done;
  transaction.status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_LOG("new local transaction status: %s", status);
        if (status == gap_transaction_waiting_accept)
        {
          BOOST_CHECK_EQUAL(recipient.state->transactions().size(), 0);
          recipient.state->connect();
          BOOST_CHECK_EQUAL(recipient.state->transactions().size(), 1);
          auto& state_transaction_recipient = *recipient.state->transactions().begin()->second;
          state_transaction_recipient.accept();
        }
        else if(status == gap_transaction_finished)
          done.open();
      });
  reactor::wait(done);
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  // suite.add(BOOST_TEST_CASE(links), 0, timeout);
  suite.add(BOOST_TEST_CASE(links_another_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(swaggers), 0, timeout);
  suite.add(BOOST_TEST_CASE(disconnect), 0, timeout);
}
