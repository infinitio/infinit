#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

// Send a file to a ghost.
ELLE_TEST_SCHEDULED(send_ghost)
{
  tests::Server server;
  auto const email = "em@il.com";
  auto const password = "secret";
  server.register_user(email, password);
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

  // Callbacks
  reactor::Barrier transferring;
  reactor::Barrier finished;
  state->attach_callback<surface::gap::PeerTransaction>(
    [&] (surface::gap::PeerTransaction const& transaction)
    {
      // Check the GUI goes created -> transferring -> finished.
      auto status = transaction.status;
      ELLE_LOG("new transaction status: %s", status);
      switch (status)
      {
        case gap_transaction_transferring:
          BOOST_CHECK(!transferring);
          BOOST_CHECK(!finished);
          transferring.open();
          break;
        case gap_transaction_ghost_uploaded:
          BOOST_CHECK(transferring);
          BOOST_CHECK(!finished);
          finished.open();
          break;
        default:
          BOOST_ERROR(elle::sprintf("unexpected GAP status: %s", status));
      }
    });
  state->login(email, password);
  std::string const ghost_email = "ghost@infinit.io";
  auto& state_transaction = state->transaction_peer_create(
    ghost_email,
    std::vector<std::string>{transfered.path().string().c_str()},
    "message");
  while (state_transaction.data()->status ==
         infinit::oracles::Transaction::Status::created)
  {
    reactor::sleep(10_ms);
    state->poll();
  }
  BOOST_CHECK_EQUAL(state_transaction.data()->status,
                    infinit::oracles::Transaction::Status::initialized);
  auto tid = state_transaction.data()->id;
  ELLE_LOG_SCOPE("transaction was initialized: %s", tid);
  while (!finished)
  {
    reactor::sleep(100_ms);
    state->poll();
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(15);
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(send_ghost), 0, timeout);
}
