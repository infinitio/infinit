#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <infinit/oracles/trophonius/Client.hh>
#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>
#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.state->test");

// Cloud buffered peer transaction.
ELLE_TEST_SCHEDULED(cloud_buffer)
{
  tests::Server server;
  auto const email = "sender@infinit.io";
  auto const password = "secret";
  auto& sender = server.register_user(email, password);

  std::string const recipient_email = "recipient@infinit.io";
  server.register_user("recipient@infinit.io", password);
  tests::State state(server, elle::UUID::random());
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
  state->login(email, password);
  auto& state_transaction = state->transaction_peer_create(
    recipient_email,
    std::vector<std::string>{transfered.path().string().c_str()},
    "message");
  reactor::Barrier transferring, cloud_buffered;
  state_transaction.status_changed().connect(
    [&] (gap_TransactionStatus status)
    {
      ELLE_LOG("new local transaction status: %s", status);
      auto& server_transaction =
        server.transaction(state_transaction.data()->id);
      switch (status)
      {
        case gap_transaction_transferring:
        {
          BOOST_CHECK_EQUAL(
            server_transaction.status,
            infinit::oracles::Transaction::Status::initialized);
          transferring.open();
          break;
        }
        case gap_transaction_cloud_buffered:
        {
          BOOST_CHECK(transferring);
          BOOST_CHECK_EQUAL(
            server_transaction.status,
            infinit::oracles::Transaction::Status::cloud_buffered);
          BOOST_CHECK(server.cloud_buffered());
          cloud_buffered.open();
          break;
        }
        default:
        {
          BOOST_FAIL(
            elle::sprintf("unexpected transaction status: %s", status));
          break;
        }
      }
    });
  reactor::wait(cloud_buffered);
  // This triggers acceptation and connection of the peer, failing the test
  // because the GUI goes in "transferring" mode even though it's already cloud
  // buffered. Uncomment and complete when it's fixed.
  // ELLE_LOG("accept transaction")
  // {
  //   state_transaction.data()->status =
  //     infinit::oracles::Transaction::Status::accepted;
  //   state_transaction.on_transaction_update(state_transaction.data());
  // }
  // ELLE_LOG("make recipient online")
  // {
  //   auto notif =
  //     elle::make_unique<infinit::oracles::trophonius::UserStatusNotification>();
  //   notif->user_id = sender.id().repr();
  //   notif->device_id = state_transaction.data()->sender_device_id;
  //   notif->user_status = true;
  //   notif->device_status = true;
  //   state->handle_notification(std::move(notif));
  // }
  reactor::sleep();
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(cloud_buffer), 0, timeout);
}
