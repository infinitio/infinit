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
  elle::filesystem::TemporaryDirectory sender_home(
    "cloud-buffer_sender_home");
  auto const& sender_user = server.register_user("sender@infinit.io", "password");
  auto const& recipient = server.register_user("recipient@infinit.io", "password");
  std::string t_id;
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
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    auto& state_transaction = sender.state->transaction_peer_create(
      recipient.email(),
      std::vector<std::string>{transfered.path().string().c_str()},
      "message");
    reactor::Barrier transferring, cloud_buffered;
    auto conn = state_transaction.status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
      t_id = state_transaction.data()->id;
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
    conn.disconnect();
    sender.logout();
  }
  auto& server_transaction = server.transaction(t_id);
  server_transaction.status = infinit::oracles::Transaction::Status::accepted;
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    sender.state->synchronize();
    for (auto& transaction : sender.state->transactions())
    {
      ELLE_LOG("transaction status on new sender: %s", transaction.second->status());
      auto data = transaction.second->data().get();
      auto peer_data = dynamic_cast<infinit::oracles::PeerTransaction*>(data);
      BOOST_CHECK(peer_data->cloud_buffered);
      transaction.second->status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_LOG("transaction status changed: %s", status);
        BOOST_CHECK(peer_data->cloud_buffered);
      });
    }
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(cloud_buffer), 0, timeout);
}
