#include <elle/filesystem/TemporaryFile.hh>
#include <elle/log.hh>
#include <elle/test.hh>

#include "server.hh"

ELLE_LOG_COMPONENT("surface.gap.state.test");

/*-------------------.
| Send to Self Limit |
`-------------------*/

namespace tests
{
  class SendToSelfServer
    : public Server
  {

  protected:
    std::string
    _transaction_post(Headers const& headers,
                     Cookies const& cookies,
                     Parameters const& parameters,
                     elle::Buffer const& body) override
    {
      std::stringstream ss;
      {
        elle::serialization::json::SerializerOut output(ss, false);
        output.serialize("error", static_cast<int32_t>(
          infinit::oracles::meta::Error::send_to_self_limit_reached));
        output.serialize(
          "reason", std::string("Send to self transaction limit reached."));
        output.serialize("limit", 1);
      }
      throw Server::Exception(
        "",
        reactor::http::StatusCode::Payment_Required,
        ss.str());
    }
  };
}

ELLE_TEST_SCHEDULED(send_to_self_limit)
{
  tests::SendToSelfServer server;
  auto const& sender_user =
    server.register_user("sender@infinit.io", "password");
  elle::filesystem::TemporaryDirectory sender_home("send-to-self_sender");
  elle::filesystem::TemporaryFile transfered("file");
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    auto& transaction = sender.state->transaction_peer_create(
      sender_user.email(),
      std::vector<std::string>{transfered.path().string().c_str()},
      "message");
    reactor::Barrier payment_required, canceled;
    transaction.status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_LOG("new local transaction status: %s", status);
        switch (status)
        {
          case gap_transaction_payment_required:
            payment_required.open();
            break;
          case gap_transaction_canceled:
            canceled.open();
            break;
          default:
            BOOST_FAIL(
              elle::sprintf("unexpected transaction status: %s", status));
            break;
        }
      });
    reactor::wait(payment_required);
    ELLE_LOG("got payment required status");
    reactor::wait(canceled);
    ELLE_LOG("got canceled status");
  }
}

/*-------------------------.
| File Size Transfer Limit |
`-------------------------*/

namespace tests
{
  class TransferSizeLimitServer
    : public Server
  {
  public:
    ELLE_ATTRIBUTE_RW(uint64_t, file_size);

  protected:
    std::string
    _transaction_put(Headers const& headers,
                     Cookies const& cookies,
                     Parameters const& parameters,
                     elle::Buffer const& body,
                     elle::UUID const&) override
    {
      elle::IOStream stream(body.istreambuf());
      elle::serialization::json::SerializerIn input(stream, false);
      uint64_t received_file_size;
      input.serialize("total_size", received_file_size);
      BOOST_CHECK_EQUAL(this->file_size(), received_file_size);
      std::stringstream ss;
      {
        elle::serialization::json::SerializerOut output(ss, false);
        output.serialize("error", static_cast<int32_t>(
          infinit::oracles::meta::Error::file_transfer_size_limited));
        output.serialize("reason",
                         std::string("File transfer size limited."));
        output.serialize("limit", this->file_size() - 1);
      }
      throw Server::Exception(
        "",
        reactor::http::StatusCode::Payment_Required,
        ss.str());
    }
  };
}

ELLE_TEST_SCHEDULED(transfer_size_limit)
{
  tests::TransferSizeLimitServer server;
  auto const& sender_user =
    server.register_user("sender@infinit.io", "password");
  elle::filesystem::TemporaryDirectory sender_home("send-to-self_sender");
  elle::filesystem::TemporaryFile transfered("file");
  uint64_t content_size = 0;
  boost::filesystem::ofstream f(transfered.path());
  std::string stuff("stuffs n ");
  for (int i = 0; i < 10000; i++)
  {
    f << stuff;
    content_size += stuff.length();
  }
  f.close();
  server.file_size(content_size);
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    auto& transaction = sender.state->transaction_peer_create(
      sender_user.email(),
      std::vector<std::string>{transfered.path().string().c_str()},
      "message");
    reactor::Barrier payment_required, canceled;
    transaction.status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_LOG("new local transaction status: %s", status);
        server.transaction(transaction.data()->id);
        switch (status)
        {
          case gap_transaction_payment_required:
            payment_required.open();
            break;
          case gap_transaction_canceled:
            canceled.open();
            break;
          default:
            BOOST_FAIL(
              elle::sprintf("unexpected transaction status: %s", status));
            break;
        }
      });
    reactor::wait(payment_required);
    ELLE_LOG("got payment required status");
    reactor::wait(canceled);
    ELLE_LOG("got canceled status");
  }
}

/*----------------.
| Link Quota Test |
`----------------*/

namespace tests
{
  class LinkQuotaServer
    : public Server
  {
  public:
    LinkQuotaServer()
      : Server()
      , _link_id(elle::UUID::random())
    {
      this->register_route(
        "/link_empty",
        reactor::http::Method::POST,
        [&] (Headers const& headers,
             Cookies const& cookies,
             Parameters const& parameters,
             elle::Buffer const& body)
        {
          std::stringstream ss;
          {
            elle::serialization::json::SerializerOut output(ss, false);
            output.serialize("created_link_id", this->link_id());
          }
          return ss.str();
        });
      this->register_route(
        elle::sprintf("/link/%s", this->link_id()),
        reactor::http::Method::PUT,
        [&] (Headers const& headers,
             Cookies const& cookies,
             Parameters const& parameters,
             elle::Buffer const& body) -> std::string
        {
          std::stringstream ss;
          {
            elle::serialization::json::SerializerOut output(ss, false);
            output.serialize("error", static_cast<int32_t>(
              infinit::oracles::meta::Error::link_storage_limit_reached));
            output.serialize("reason",
                             std::string("Link storage limit reached."));
            output.serialize("quota", 100);
            output.serialize("usage", 99);
          }
          throw Server::Exception(
            "",
            reactor::http::StatusCode::Payment_Required,
            ss.str());
        });
      this->register_route(
        elle::sprintf("/link/%s", this->link_id()),
        reactor::http::Method::POST,
        [&] (Server::Headers const&,
             Server::Cookies const& cookies,
             Server::Parameters const& parameters,
             elle::Buffer const& body) -> std::string
        {
          return "{\"success\": true}";
        });
    }

  public:
    ELLE_ATTRIBUTE_R(elle::UUID, link_id);
  };
}

ELLE_TEST_SCHEDULED(link_storage_limit_reached)
{
  tests::LinkQuotaServer server;
  auto const& sender_user =
    server.register_user("sender@infinit.io", "password");
  elle::filesystem::TemporaryDirectory sender_home("send-to-self_sender");
  elle::filesystem::TemporaryFile transfered("file");
  uint64_t content_size = 0;
  boost::filesystem::ofstream f(transfered.path());
  std::string stuff("stuffs n ");
  for (int i = 0; i < 10000; i++)
  {
    f << stuff;
    content_size += stuff.length();
  }
  f.close();
  {
    tests::Client sender(server, sender_user, sender_home.path());
    sender.login();
    uint32_t link_id = sender.state->create_link(
      std::vector<std::string>{transfered.path().string()},
      "message",
      false);
    auto& link = sender.state->transactions().at(link_id);
    reactor::Barrier payment_required, canceled;
    link->status_changed().connect(
      [&] (gap_TransactionStatus status)
      {
        ELLE_LOG("new local link status: %s", status);
        switch (status)
        {
          case gap_transaction_payment_required:
            payment_required.open();
            break;
          case gap_transaction_canceled:
            canceled.open();
            break;
          default:
            BOOST_FAIL(
              elle::sprintf("unexpected link status: %s", status));
            break;
        }
      });
    reactor::wait(payment_required);
    ELLE_LOG("got payment required status");
    reactor::wait(canceled);
    ELLE_LOG("got canceled status");
  }
}


ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 60 : 15;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(send_to_self_limit), 0, timeout);
  suite.add(BOOST_TEST_CASE(transfer_size_limit), 0, timeout);
  suite.add(BOOST_TEST_CASE(link_storage_limit_reached), 0, timeout);
}
