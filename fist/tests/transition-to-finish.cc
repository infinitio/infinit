#include <boost/lexical_cast.hpp>
#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>
#include <surface/gap/enums.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.Stage.test");

ELLE_TEST_SCHEDULED(wait_to_finish)
{
  tests::Server server;
  auto const& sender = server.register_user("sender@infinit.io", "password");
  auto const& recipient = server.register_user("recipient@infinit.io", "password");
  elle::filesystem::TemporaryFile transfered("filename");
  elle::filesystem::TemporaryDirectory sender_home(
    "transition-to-finish_sender_home");
  elle::filesystem::TemporaryDirectory recipient_home(
    "transition-to-finish_recipient_home");
  std::string t_id;
  uint32_t id;
  ELLE_LOG("send file with recipient offline")
  {
    tests::Client sender_client(server, sender, sender_home.path());
    sender_client.login();
    auto& transaction = sender_client.state->transaction_peer_create(
      recipient.email(),
      std::vector<std::string>{transfered.path().string()},
      "message");
    reactor::Barrier cloud_buffered("cloud buffered");
    auto conn = transaction.status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_WARN("transaction status changed: %s", status);
        t_id = transaction.data()->id;
        id = transaction.id();
        auto& server_t = server.transaction(transaction.data()->id);
        if (status == gap_transaction_cloud_buffered)
          cloud_buffered.open();
      });
    reactor::wait(cloud_buffered);
    conn.disconnect();
    sender_client.logout();
  }
  server.transaction(t_id).status = infinit::oracles::Transaction::Status::finished;
  ELLE_LOG("sender reconnects, wait for status to change to finished")
  {
    tests::Client sender_client(server, sender, sender_home.path());
    sender_client.login();
    sender_client.state->synchronize();
    reactor::Barrier finished("finished");
    for (auto& transaction : sender_client.state->transactions())
    {
      ELLE_WARN("transaction status on new sender: %s", *transaction.second);
      if (transaction.second->status() == gap_transaction_finished)
        finished.open();
      transaction.second->status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_WARN("transaction status changed: %s", status);
        if (status == gap_transaction_finished)
          finished.open();
      });
    }
    reactor::wait(finished);
  }
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(wait_to_finish), 0, 30);
}
