#include <boost/uuid/random_generator.hpp>

#include <common/common.hh>

#include <elle/log.hh>
#include <elle/test.hh>
#include <elle/AtomicFile.hh>
#include <elle/filesystem/TemporaryFile.hh>
#include <elle/serialization/json.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/Transaction.hh>
#include <surface/gap/State.hh>
#include <surface/gap/enums.hh>

#include <infinit/oracles/PeerTransaction.hh>
#include <infinit/oracles/trophonius/Client.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("fist.multi_device.tests");

static
void
inspect_snapshots(Server::Client& client,
                    Server::Transaction const& tr,
                    int expected_count = 1,
                    infinit::oracles::Transaction::Status expected_status = infinit::oracles::Transaction::Status::initialized)
{
  boost::filesystem::path snapshots_path(
    common::infinit::user_directory(client.state.home(), boost::lexical_cast<std::string>(client.user.id())));
  snapshots_path /= "transactions";
  {
    int count = 0;
    create_directories(snapshots_path);
    using boost::filesystem::directory_iterator;
    for (auto it = directory_iterator(snapshots_path);
         it != directory_iterator();
         ++it)
    {
      ++count;
      auto snapshot_path = boost::filesystem::path(it->path());
      elle::AtomicFile source(snapshot_path / "transaction.snapshot");
      if (!source.exists())
        throw elle::Error("transaction snapshot is missing.");
      surface::gap::Transaction::Snapshot snapshot = [&] () {
        return source.read() << [&] (elle::AtomicFile::Read& read)
        {
          using namespace elle::serialization;
          json::SerializerIn input(read.stream(), false);
          return surface::gap::Transaction::Snapshot(input);
        };
      }();
      ELLE_ASSERT_EQ(snapshot.data()->status, infinit::oracles::Transaction::Status::initialized);
    }
    ELLE_ASSERT_EQ(count, expected_count);
  }
}

ELLE_TEST_SCHEDULED(snapshot_leftovers)
{
  elle::filesystem::TemporaryFile file("lulz");
  {
    std::ofstream f{file.path().string()};
    f << "stuff\n";
  }

  Server server;
  auto& bob = server.register_user("recipient@infinit.io", "password");
  Server::Client bob_device1(server, bob);
  Server::Client bob_device2(server, bob);
  bob_device1.login();
  bob_device2.login();
  ELLE_ASSERT(bob_device1.state.device().id != bob_device2.state.device().id);
  Server::Client alice(server, "alice@infinit.io");
  alice.login();
  reactor::Signal logout_device2;
  reactor::Barrier received_on_device1, received_on_device2;

  bob_device1.state.attach_callback<surface::gap::Transaction::Notification>(
    [&] (surface::gap::Transaction::Notification const& notif)
    {
      // Check the GUI goes created -> transferring -> finished.
      auto status = notif.status;
      switch (status)
      {
        case gap_transaction_waiting_accept:
          received_on_device1.open();
          break;
        default:
         break;
      }
    });

  bob_device2.state.attach_callback<surface::gap::Transaction::Notification>(
    [&] (surface::gap::Transaction::Notification const& notif)
    {
      // Check the GUI goes created -> transferring -> finished.
      auto status = notif.status;
      switch (status)
      {
        case gap_transaction_waiting_accept:
          received_on_device2.open();
          break;
  default:
    break;
      }
    });

  alice.state.attach_callback<surface::gap::Transaction::Notification>(
    [&] (surface::gap::Transaction::Notification const& notif)
    {
      // Check the GUI goes created -> transferring -> finished.
      auto status = notif.status;
      switch (status)
      {
        case gap_transaction_waiting_accept:
          received_on_device2.open();
          break;
  default:
    break;
      }
    });

  alice.state.transaction_peer_create(
    "recipient@infinit.io", std::vector<std::string>{file.path().string()}, "message");

  while (server.transactions().size() == 0)
    reactor::yield();

  while (true)
  {
    bool stop = false;
    for (auto& tr: server.transactions().get<0>())
      if (tr->status == infinit::oracles::Transaction::Status::initialized)
        stop = true;
    if (stop)
      break;
    reactor::yield();
  }

  std::string transaction_id;
  for (auto& tr: server.transactions().get<0>())
    transaction_id = tr->id;

  auto const& tr = server.transactions().get<0>().find(transaction_id);

  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background(
      "bob device 1",
      [&]
      {
        while (true)
        {
          bob_device1.state.poll();
          reactor::yield();
        }
      });
    scope.run_background(
      "bob device 2",
      [&]
      {
        while (true)
        {
          bob_device2.state.poll();
          reactor::yield();
        }
      });
    scope.run_background(
      "alice",
      [&]
      {
        while (true)
        {
          alice.state.poll();
          reactor::yield();
        }
      });
    reactor::wait(received_on_device1);
    reactor::wait(received_on_device2);
  };

  inspect_snapshots(bob_device1, **tr);
  inspect_snapshots(bob_device2, **tr);

  bob_device1.logout();
  bob_device2.logout();

  bob_device1.login();
  bob_device2.login();

  inspect_snapshots(bob_device1, **tr);
  inspect_snapshots(bob_device2, **tr);

  ELLE_ASSERT(bob_device1.state.transactions().size() == 1);
  ELLE_ASSERT(bob_device2.state.transactions().size() == 1);
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(snapshot_leftovers), 0, 10);
}
