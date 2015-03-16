#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.State.test");

class CreateEmptyServer
  : public tests::Server
{
protected:
  virtual
  elle::UUID
  _create_empty() override
  {
    this->_created.open();
    reactor::sleep();
    return elle::UUID();
  }
  ELLE_ATTRIBUTE_RX(reactor::Barrier, created);
};

ELLE_TEST_SCHEDULED(create_transaction)
{
  CreateEmptyServer server;
  auto const& sender = server.register_user("sender@infinit.io", "password");
  auto const& recipient = server.register_user("recipient@infinit.io", "password");
  elle::filesystem::TemporaryFile transfered("filename");
  elle::filesystem::TemporaryDirectory home(
    "snapshot-resume_create-transaction");
  ELLE_LOG("first session")
  {
    tests::Client sender_client(server, sender, home.path());
    sender_client.login();
    ELLE_LOG("create transaction")
      sender_client.state.transaction_peer_create(
        recipient.email(),
        std::vector<std::string>{transfered.path().string()},
        "message");
    reactor::wait(server.created());
    ELLE_LOG("shutdown state");
  }
  server.created().close();
  ELLE_LOG("second session")
  {
    tests::Client sender_client(server, sender, home.path());
    sender_client.login();
    reactor::wait(server.created());
    ELLE_LOG("shutdown state");
  }
}

class InitializeTransactionServer
  : public tests::Server
{
public:
  typedef tests::Server Super;

protected:
  elle::UUID
  _create_empty() override
  {
    BOOST_CHECK(!this->_created);
    this->_created.open();
    this->_id = Super::_create_empty();
    return this->_id;
  }

  std::string
  _transaction_put(Server::Headers const& headers,
                   Server::Cookies const& cookies,
                   Server::Parameters const& params,
                   elle::Buffer const& content,
                   elle::UUID const& id) override
  {

    BOOST_CHECK_EQUAL(id, this->_id);
    Super::_transaction_put(headers, cookies, params, content, id);
    this->_initialized.open();
    reactor::sleep();
    return "unused";
  }

  ELLE_ATTRIBUTE_RX(elle::UUID, id);
  ELLE_ATTRIBUTE_RX(reactor::Barrier, created);
  ELLE_ATTRIBUTE_RX(reactor::Barrier, initialized);
};

ELLE_TEST_SCHEDULED(initialize_transaction)
{
  InitializeTransactionServer server;
  auto const& sender = server.register_user("sender@infinit.io", "password");
  auto const& recipient = server.register_user("recipient@infinit.io", "password");
  elle::filesystem::TemporaryFile transfered("filename");
  elle::filesystem::TemporaryDirectory home(
    "snapshot-resume_initialize-transaction");
  ELLE_LOG("first session")
  {
    tests::Client sender_client(server, sender, home.path());
    sender_client.login();
    ELLE_LOG("create transaction")
      sender_client.state.transaction_peer_create(
        recipient.email(),
        std::vector<std::string>{transfered.path().string()},
        "message");
    reactor::wait(server.initialized());
    ELLE_LOG("shutdown state");
  }
  server.initialized().close();
  ELLE_LOG("second session")
  {
    tests::Client sender_client(server, sender, home.path());
    sender_client.login();
    reactor::wait(server.initialized());
    ELLE_LOG("shutdown state");
  }
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 30 : 10;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(create_transaction), 0, timeout);
  suite.add(BOOST_TEST_CASE(initialize_transaction), 0, timeout);
}
