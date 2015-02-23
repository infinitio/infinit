#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

class CreateEmptyServer
  : public Server
{
protected:
  virtual
  boost::uuids::uuid
  _create_empty()
  {
    this->_created.open();
    reactor::sleep();
    return boost::uuids::uuid();
  }

  ELLE_ATTRIBUTE_RX(reactor::Barrier, created);
};

ELLE_TEST_SCHEDULED(create_empty)
{
  CreateEmptyServer server;
  auto sender = server.register_user("sender@infinit.io", "password");
  auto recipient = server.register_user("recipient@infinit.io", "password");
  elle::filesystem::TemporaryFile transfered("filename");
  elle::filesystem::TemporaryDirectory home("snapshot-resume");
  ELLE_LOG("first session")
  {
    Server::Client sender_client(server, sender, home.path());
    sender_client.login();
    ELLE_LOG("create transaction")
      auto& state_transaction = sender_client.state.transaction_peer_create(
        recipient.email(),
        std::vector<std::string>{transfered.path().string()},
        "message");
    reactor::wait(server.created());
    ELLE_LOG("shutdown state");
  }
  server.created().close();
  ELLE_LOG("second session")
  {
    Server::Client sender_client(server, sender, home.path());
    sender_client.login();
    reactor::wait(server.created());
    ELLE_LOG("shutdown state");
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 30 : 10;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(create_empty), 0, timeout);
}
