#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include <surface/gap/Exception.hh>
#include <surface/gap/State.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

// Send a file to a ghost.
ELLE_TEST_SCHEDULED(ghost_download)
{
  // FIXME: check statuses sent to meta
  tests::Server server;
  server.register_route(
    "/ghost-download",
    reactor::http::Method::GET,
    [] (tests::Server::Headers const&,
        tests::Server::Cookies const&,
        tests::Server::Parameters const&,
        elle::Buffer const&)
    {
      return "Boo";
    });
  auto const email = "em@il.com";
  auto const password = "secret";
  auto& sender = server.register_user("sender@infinit.io", password);
  auto& user = server.register_user(email, password);
  tests::State state(server, elle::UUID::random());

  auto t = std::make_shared<tests::Transaction>();
  t->recipient_id = user.id().repr();
  t->sender_id = sender.id().repr();
  t->is_ghost = true;
  t->download_link =
    elle::sprintf("http://127.0.0.1:%s/ghost-download", server.port());
  // FIXME: also test changing to ghost uploaded later
  t->status = infinit::oracles::Transaction::Status::ghost_uploaded;
  server.transactions().insert(t);

  reactor::Barrier accepted;
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
        case gap_transaction_waiting_accept:
          BOOST_CHECK(!accepted);
          BOOST_CHECK(!transferring);
          BOOST_CHECK(!finished);
          auto transaction = state->transactions().at(transaction.id);
          transaction->accept();
          accepted.open();
          break;
        case gap_transaction_transferring:
          BOOST_CHECK(accept);
          BOOST_CHECK(!transferring);
          BOOST_CHECK(!finished);
          BOOST_CHECK_EQUAL(elle::UUID(transaction.recipient_device_id),
                            state->device_uuid());
          transferring.open();
          break;
        case gap_transaction_finished:
          BOOST_CHECK(accept);
          BOOST_CHECK(transferring);
          BOOST_CHECK(!finished);
          finished.open();
          // FIXME: check the files containse 'Boo'
          break;
        default:
          BOOST_ERROR(elle::sprintf("unexpected GAP status: %s", status));
      }
    });
  state->login(email, password);
  while (!finished)
  {
    reactor::sleep(100_ms);
    state->poll();
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 15;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(ghost_download), 0, timeout);
}
