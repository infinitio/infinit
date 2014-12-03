#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

// Cloud buffered peer transaction.
ELLE_TEST_SCHEDULED(cloud_buffer)
{
  Server server;
  auto const email = "sender@infinit.io";
  auto const password = "secret";
  auto user = server.register_user(email, password);

  std::string const recipient_email = "recipient@infinit.io";
  auto recipient = server.register_user("recipient@infinit.io", password);
  State state(server, user.device_id());

  state.login(email, password);
  auto& state_transaction = state.transaction_peer_create(
    recipient_email,
    std::vector<std::string>{"/etc/passwd"},
    "message");
  while (state_transaction.data()->status ==
         infinit::oracles::Transaction::Status::created)
  {
    reactor::sleep(10_ms);
    state.poll();
  }
  BOOST_CHECK_EQUAL(state_transaction.data()->status,
                    infinit::oracles::Transaction::Status::initialized);
  auto& server_transaction = server.transaction(state_transaction.data()->id);
  BOOST_CHECK_EQUAL(server_transaction.status(),
                    infinit::oracles::Transaction::Status::initialized);

  // Callbacks
  reactor::Barrier transferring, cloud_buffered;
  state_transaction.status_changed().connect(
    [&] (gap_TransactionStatus status)
    {
      ELLE_LOG("new local transaction status: %s", status);
      switch (status)
      {
        case gap_transaction_transferring:
          BOOST_CHECK_EQUAL(
            server_transaction.status(),
            infinit::oracles::Transaction::Status::initialized);
          transferring.open();
          break;
        case gap_transaction_cloud_buffered:
          BOOST_CHECK(transferring);
          BOOST_CHECK_EQUAL(
            server_transaction.status(),
            infinit::oracles::Transaction::Status::cloud_buffered);
          BOOST_CHECK(server.cloud_buffered());
          cloud_buffered.open();
          break;
        default:
          BOOST_FAIL(
            elle::sprintf("unexpected transaction status: %s", status));
          break;
      }
    });
  server_transaction.status_changed().connect(
    [&] (infinit::oracles::Transaction::Status status)
    {
      ELLE_LOG("new server transaction status: %s", status);
    });
  reactor::wait(cloud_buffered);
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 15;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(cloud_buffer), 0, timeout);
}
