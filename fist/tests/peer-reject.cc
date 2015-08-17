#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <infinit/oracles/trophonius/Client.hh>
#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>
#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.state->test");


ELLE_TEST_SCHEDULED(peer_reject)
{
  tests::Server server;
  auto const& sender_user =
    server.register_user("sender@infinit.io", "password");
  elle::filesystem::TemporaryDirectory sender_home("peer-reject_sender");
  auto const& recipient_user =
    server.register_user("recipient@infinit.io", "password");
  elle::filesystem::TemporaryDirectory recipient_home("peer-reject_recipient");
  elle::filesystem::TemporaryFile transfered("cloud-buffered");
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    tests::Client recipient(server, recipient_user, recipient_home.path());
    recipient.login();
    auto& transaction = sender.state->transaction_peer_create(
      recipient_user.email(),
      std::vector<std::string>{transfered.path().string()},
      "message");
    reactor::Barrier waiting, rejected;
    auto conn = transaction.status_changed().connect(
      [&] (gap_TransactionStatus status, boost::optional<gap_Status>)
      {
        ELLE_LOG("new local transaction status: %s", status);
        server.transaction(transaction.data()->id);
        switch (status)
        {
          case gap_transaction_waiting_accept:
          {
            BOOST_CHECK(!waiting);
            BOOST_CHECK(!rejected);
            waiting.open();
            break;
          }
          case gap_transaction_rejected:
          {
            BOOST_CHECK(waiting);
            BOOST_CHECK(!rejected);
            rejected.open();
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
    reactor::wait(waiting);
    transaction.data()->status =
      infinit::oracles::Transaction::Status::rejected;
    transaction.on_transaction_update(transaction.data());
    reactor::wait(rejected);
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 15;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(peer_reject), 0, timeout);
}
