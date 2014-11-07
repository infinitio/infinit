#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

// Send a file to a ghost.
ELLE_TEST_SCHEDULED(login)
{
  Server server;
  auto const email = "em@il.com";
  auto const password = "secret";
  auto user = server.register_user(email, password);
  State state(server, user.device_id());
  // Callbacks
  reactor::Barrier transferring;
  reactor::Barrier finished;
  state.attach_callback<surface::gap::Transaction::Notification>(
    [&] (surface::gap::Transaction::Notification const& notif)
    {
      // Check the GUI goes created -> transferring -> finished.
      auto status = notif.status;
      ELLE_LOG("new transaction status: %s", status);
      switch (status)
      {
        case gap_transaction_transferring:
          BOOST_CHECK(!transferring);
          BOOST_CHECK(!finished);
          transferring.open();
          break;
        case gap_transaction_finished:
          BOOST_CHECK(transferring);
          BOOST_CHECK(!finished);
          finished.open();
          break;
        default:
          BOOST_ERROR(elle::sprintf("unexpected GAP status: %s", status));
      }
    });
  state.login(email, password);
  std::string const ghost_email = "ghost@infinit.io";
  server.generate_ghost_user(ghost_email);
  auto local_tid = state.send_files(ghost_email,
                                    std::vector<std::string>{"/etc/passwd"},
                                    "message");
  auto& state_transaction = *state.transactions().at(local_tid);
  while (state_transaction.data()->status ==
         infinit::oracles::Transaction::Status::created)
  {
    reactor::sleep(100_ms);
    state.poll();
  }
  BOOST_CHECK_EQUAL(state_transaction.data()->status,
                    infinit::oracles::Transaction::Status::initialized);
  auto tid = state.transactions().at(local_tid)->data()->id;
  ELLE_LOG_SCOPE("transaction was initialized: %s", tid);
  while (!finished)
  {
    reactor::sleep(100_ms);
    state.poll();
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 20 : 5;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(login), 0, timeout);
}
