#include <boost/uuid/random_generator.hpp>

#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

static
void
synchronize(Server& server, surface::gap::State& state)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("reseter", [&] { reactor::sleep(300_ms); server.trophonius.disconnect_all_users(); });
    scope.run_background("synchronized", [&] { reactor::wait(state.synchronized()); });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(links)
{
  Server server;
  Server::Client sender(server, "sender@infinit.io");
  sender.login();
  ELLE_ASSERT_EQ(sender.state.transactions().size(), 0);
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
  sender.state.create_link(
    std::vector<std::string>{transfered.path().string().c_str()}, "message");
  // Because our trophonius is a brick, we will not receive the notification.
  ELLE_ASSERT_EQ(sender.state.transactions().size(), 1);
  // At the stage, status is new.
  ELLE_ASSERT_EQ(sender.state.transactions().begin()->second->data()->status,
                 infinit::oracles::Transaction::Status::created);

  while (true)
  {
    // At this stage, status has been set locally.
    if (sender.state.transactions().begin()->second->data()->status == infinit::oracles::Transaction::Status::finished)
      break;
    reactor::yield();
  }
  ELLE_ASSERT_EQ(sender.user.links.begin()->status, oracles::Transaction::Status::finished);

  sender.user.links.begin()->status =  oracles::Transaction::Status::canceled;
  // Disconnect trophonius.
  synchronize(server, sender.state);
  // At this stage, state should have resynchronization
  ELLE_ASSERT_EQ(sender.state.transactions().begin()->second->data()->status,
                 oracles::Transaction::Status::canceled);
}

ELLE_TEST_SCHEDULED(links_another_device)
{
  Server server;
  Server::Client sender(server, "sender@infinit.io");
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
  t.id = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
  t.ctime = 2173213;
  t.sender_id = boost::lexical_cast<std::string>(sender.user.id());
  t.sender_device_id = boost::lexical_cast<std::string>(sender.user.device_id()) + "other";
  t.status = infinit::oracles::Transaction::Status::initialized;
  sender.user.links.push_back(t);

  // Disconnect trophonius.
  synchronize(server, sender.state);

  ELLE_ASSERT_EQ(sender.state.transactions().begin()->second->data()->status,
                 oracles::Transaction::Status::initialized);

  t.status = infinit::oracles::Transaction::Status::finished;
  sender.user.links.clear();
  sender.user.links.push_back(t);

  synchronize(server, sender.state);

  ELLE_ASSERT_EQ(sender.state.transactions().begin()->second->data()->status,
                 oracles::Transaction::Status::finished);
}

ELLE_TEST_SCHEDULED(swaggers)
{
  Server server;
  Server::Client bob(server, "bob@infinit.io");
  Server::Client alice(server, "alice@infinit.io");
  Server::Client eve(server, "eve@infinit.io");

  alice.user.swaggers.insert(&bob.user);
  bob.user.swaggers.insert(&alice.user);

  alice.login();
  bob.login();
  ELLE_ASSERT_EQ(bob.state.swaggers().size(), 1);

  eve.user.swaggers.insert(&bob.user);
  bob.user.swaggers.insert(&eve.user);

  bool step0 = true; // Alice was logged in before (no notification).
  bool step1 = false;
  bob.state.attach_callback<surface::gap::State::UserStatusNotification>([&] (surface::gap::State::UserStatusNotification notif)
  {
    step0 = false;
    auto it = bob.state.users().find(notif.id);
    if (it->second.id == boost::lexical_cast<std::string>(eve.user.id()))
    {
      step1 = notif.status;
    }
  });
  synchronize(server, bob.state);
  bob.state.poll();
  ELLE_ASSERT(step0); // Nothing should have changed.
  ELLE_ASSERT(!step1); // Eve is a swagger but not online.
  for (auto const& user: server.users().get<0>())
    ELLE_WARN("%s", *user);
  ELLE_ASSERT_EQ(bob.state.swaggers().size(), 2);
  eve.login();
  synchronize(server, bob.state);
  bob.state.poll();
  ELLE_ASSERT(!step0); // It should be valid now.
  ELLE_ASSERT(step1); // It should be valid now.
  ELLE_ASSERT_EQ(bob.state.swaggers().size(), 2);

  // Disclaimer: It's really hard to play with multi device on a single device
  // when playing with real states. I'll use some random device ids to check
  // behaviours.
  bob.user.connected_devices.clear();
  bob.user.connected_devices.insert(boost::uuids::random_generator()());
  bob.user.connected_devices.insert(boost::uuids::random_generator()());

  bool step2 = false;
  alice.state.attach_callback<surface::gap::State::UserStatusNotification>([&] (surface::gap::State::UserStatusNotification notif)
  {
    if (step2) return;

    ELLE_ERR("%s", notif.id);
    auto it = alice.state.users().find(notif.id);
    if (it->second.id == boost::lexical_cast<std::string>(bob.user.id()))
    {
      step2 = notif.status;
    }
  });
  synchronize(server, alice.state);
  alice.state.poll();
  ELLE_ASSERT(step2);

  bob.user.connected_devices.clear();
  bob.logout();

  bool step3 = false;
  alice.state.attach_callback<surface::gap::State::UserStatusNotification>([&] (surface::gap::State::UserStatusNotification notif)
  {
    auto it = alice.state.users().find(notif.id);
    if (it->second.id == boost::lexical_cast<std::string>(bob.user.id()))
    {
      step3 = !notif.status;
    }
  });

  synchronize(server, alice.state);

  alice.state.poll();
  ELLE_ASSERT(step3);
}


ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 15;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(links), 0, timeout);
  suite.add(BOOST_TEST_CASE(links_another_device), 0, timeout);
  suite.add(BOOST_TEST_CASE(swaggers), 0, timeout);
}
