#include <boost/uuid/uuid_io.hpp>

#include <elle/log.hh>
#include <elle/test.hh>
#include <elle/utility/Move.hh>

#include <reactor/Scope.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

#include <infinit/oracles/trophonius/server/Trophonius.hh>
#include <infinit/oracles/trophonius/server/utils.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.test")

using infinit::oracles::trophonius::server::pretty_print_json;
using infinit::oracles::trophonius::server::read_json;
using infinit::oracles::trophonius::server::write_json;

class Meta
{
public:
  Meta():
    _server(),
    _port(0),
    _accepter()
  {
    this->_server.listen(0);
    this->_port = this->_server.port();
    ELLE_LOG("%s: listen on port %s", *this, this->_port);
    this->_accepter.reset(
      new reactor::Thread(*reactor::Scheduler::scheduler(),
                          "accepter",
                          std::bind(&Meta::_accept,
                                    std::ref(*this))));
  }

  ~Meta()
  {
    this->_accepter->terminate_now();
  }

  std::string
  url(std::string const& path)
  {
    return elle::sprintf("http://127.0.0.1:%s/%s", this->port(), path);
  }

  typedef std::pair<std::string, std::string> Client;
  typedef std::unordered_set<Client, boost::hash<Client>> Clients;
  typedef std::pair<int, Clients> Trophonius;
  typedef std::unordered_map<std::string, Trophonius> Trophoniuses;
  ELLE_ATTRIBUTE(reactor::network::TCPServer, server);
  ELLE_ATTRIBUTE_R(int, port);
  ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter);
  ELLE_ATTRIBUTE_R(Trophoniuses, trophoniuses);

  Clients&
  trophonius(std::string const& id)
  {
    BOOST_CHECK(this->_trophoniuses.find(id) != this->_trophoniuses.end());
    return this->_trophoniuses.find(id)->second.second;
  }

  Clients&
  trophonius(infinit::oracles::trophonius::server::Trophonius const& t)
  {
    return this->trophonius(boost::lexical_cast<std::string>(t.uuid()));
  }

  void
  _accept()
  {
    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      while (true)
      {
        std::unique_ptr<reactor::network::TCPSocket> socket(
          this->_server.accept());
        ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
        auto name = elle::sprintf("request %s", socket->peer());
        scope.run_background(
          name,
          std::bind(&Meta::_serve, std::ref(*this),
                    elle::utility::move_on_copy(socket)));
      }
    };
  }

  void
  _response_success(reactor::network::TCPSocket& socket)
  {
    this->response(socket,
                   std::string("{\"success\": true }"));
  }

  void
  _response_failure(reactor::network::TCPSocket& socket)
  {
    this->response(socket,
                   std::string("{"
                               "  \"success\": false,"
                               "  \"error_code\": 0,"
                               "  \"error_details\": \"fuck you.\""
                               "}"));
  }

  void
  _serve(std::unique_ptr<reactor::network::TCPSocket> socket)
  {
    auto peer = socket->peer();
    {
      auto request = socket->read_until("\r\n");
      ELLE_TRACE("%s: got request from %s: %s", *this, peer, request.string());
      std::vector<std::string> words;
      boost::algorithm::split(words, request,
                              boost::algorithm::is_any_of(" "));
      std::string method = words[0];
      std::string path = words[1];
      // Read remaining headers.
      ELLE_DEBUG("%s: read remaining headers", *this)
        socket->read_until("\r\n\r\n");
      {
        std::vector<std::string> chunks;
        boost::algorithm::split(chunks, path,
                                boost::algorithm::is_any_of("/"));
        BOOST_CHECK_GE(chunks.size(), 3);
        BOOST_CHECK_EQUAL(chunks[0], "");
        BOOST_CHECK_EQUAL(chunks[1], "trophonius");
        std::string id = chunks[2];
        if (chunks.size() == 3)
        {
          if (method == "PUT")
          {
            auto json = read_json(*socket);
            BOOST_CHECK(json.find("port") != json.end());
            this->_register(*socket, id, json.find("port")->second.getInt());
          }
          else if (method == "DELETE")
            this->_unregister(*socket, id);
          this->response(*socket,
                         std::string("{\"success\": true }"));
          return;
        }
        BOOST_CHECK_EQUAL(chunks.size(), 6);
        BOOST_CHECK_EQUAL(chunks[3], "users");
        std::string user = chunks[4];
        std::string device = chunks[5];
        if (method == "PUT")
          this->_register_user(*socket, id, user, device);
        else if (method == "DELETE")
          this->_unregister(*socket, id, user, device);
        return;
      }
    }
  }

  virtual
  void
  _register(reactor::network::TCPSocket& socket,
            std::string const& id,
            int port)
  {
    ELLE_LOG_SCOPE("%s: register trophonius %s on port %s", *this, id, port);
    BOOST_CHECK(this->_trophoniuses.find(id) == this->_trophoniuses.end());
    this->_trophoniuses.insert(std::make_pair(id, Trophonius(port, Clients())));
  }

  virtual
  void
  _unregister(reactor::network::TCPSocket& socket,
            std::string const& id)
  {
    ELLE_LOG_SCOPE("%s: unregister trophonius %s", *this, id);
    BOOST_CHECK(this->_trophoniuses.find(id) != this->_trophoniuses.end());
    this->_trophoniuses.erase(id);
  }

  virtual
  void
  _register_user(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: register user %s:%s on %s", *this, user, device, id);
    Client c(user, device);
    auto& trophonius = this->trophonius(id);
    BOOST_CHECK(trophonius.find(c) == trophonius.end());
    trophonius.insert(c);
    this->_response_success(socket);
  }

  virtual
  void
  _unregister(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: unregister user %s:%s on %s", *this, user, device, id);
    Client c(user, device);
    auto& trophonius = this->trophonius(id);
    BOOST_CHECK(trophonius.find(c) != trophonius.end());
    trophonius.erase(c);
    this->_response_success(socket);
  }

  void
  response(reactor::network::TCPSocket& socket,
           elle::ConstWeakBuffer content)
  {
    std::string answer(
      "HTTP/1.1 200 OK\r\n"
      "Server: Custom HTTP of doom\r\n"
      "Content-Length: " + std::to_string(content.size()) + "\r\n");
    answer += "\r\n" + content.string();
    ELLE_TRACE("%s: send response to %s: %s", *this, socket.peer(), answer);
    socket.write(elle::ConstWeakBuffer(answer));
  }

  void
  send_notification(int type, std::string user, std::string device)
  {
    try
    {
      json_spirit::Object response;
      response["user_id"] = boost::lexical_cast<std::string>(user);
      response["device_id"] = boost::lexical_cast<std::string>(device);
      json_spirit::Object notification;
      notification["notification_type"] = type;
      response["notification"] = notification;
      // XXX: hardcoded trophonius
      auto port = this->_trophoniuses.begin()->second.first;
      ELLE_LOG("%s: send notification: %s", *this, pretty_print_json(response))
      {
        reactor::network::TCPSocket notify("127.0.0.1", port);
        write_json(notify, response);
      }
    }
    catch (...)
    {
      ELLE_ERR("%s: error while sending notification: %s",
               *this, elle::exception_string());
      throw;
    }
  }
};

static
std::ostream&
operator << (std::ostream& s, Meta const& meta)
{
  s << "Meta(" << meta.port() << ")";
  return s;
}

static
void
authentify(reactor::network::TCPSocket& socket,
           int user = 0,
           int device = 0)
{
  std::string const auth =
    elle::sprintf(
      "{"
      "  \"user_id\":    \"00000000-0000-0000-0000-00000000000%s\","
      "  \"device_id\":  \"00000000-0000-0000-0000-00000000000%s\","
      "  \"session_id\": \"00000000-0000-0000-0000-000000000000\""
      "}\n", user, device);
  socket.write(auth);
}

static
void
check_authentication_success(reactor::network::TCPSocket& socket)
{
  auto json = read_json(socket);
  BOOST_CHECK_EQUAL(json["notification_type"].getInt(), -666);
  BOOST_CHECK_EQUAL(json["response_code"].getInt(), 200);
}

static
void
check_authentication_failure(reactor::network::TCPSocket& socket)
{
  auto json = read_json(socket);
  BOOST_CHECK_EQUAL(json["notification_type"].getInt(), -666);
  BOOST_CHECK_EQUAL(json["response_code"].getInt(), 403);
}

/*--------------------.
| register_unregister |
`--------------------*/

// Test registering and unregistering a trophonius from meta.

ELLE_TEST_SCHEDULED(register_unregister)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.trophoniuses().size(), 0);
  {
    ELLE_LOG("register trophonius");
    infinit::oracles::trophonius::server::Trophonius trophonius(
      0,
      "localhost",
      meta.port(),
      0,
      60_sec,
      300_sec);
    BOOST_CHECK_EQUAL(meta.trophoniuses().size(), 1);
    ELLE_LOG("unregister trophonius");
  }
  BOOST_CHECK_EQUAL(meta.trophoniuses().size(), 0);
}

/*--------------.
| notifications |
`--------------*/

// Check notifications are routed correctly.

ELLE_TEST_SCHEDULED(notifications)
{
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    8080, // XXX: hardcoded port
    60_sec,
    300_sec);
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    auto client = [&] (int user, int device, reactor::Barrier& b)
    {
      reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
      authentify(socket, user, device);
      check_authentication_success(socket);
      b.open();
      auto notif = read_json(socket);
      BOOST_CHECK_EQUAL(notif["notification_type"].getInt(),
                        user * 10 + device);
    };
    reactor::Barrier b00;
    scope.run_background("client 1:1", std::bind(client, 1, 1, std::ref(b00)));
    reactor::Barrier b01;
    scope.run_background("client 1:2", std::bind(client, 1, 2, std::ref(b01)));
    reactor::Barrier b10;
    scope.run_background("client 2:1", std::bind(client, 2, 1, std::ref(b10)));
    reactor::Barrier b11;
    scope.run_background("client 2:2", std::bind(client, 2, 2, std::ref(b11)));
    reactor::wait(reactor::Waitables({&b00, &b01, &b10, &b11}));
    try
    {
      meta.send_notification(11,
                             "00000000-0000-0000-0000-000000000001",
                             "00000000-0000-0000-0000-000000000001");
      meta.send_notification(12,
                             "00000000-0000-0000-0000-000000000001",
                             "00000000-0000-0000-0000-000000000002");
      meta.send_notification(21,
                             "00000000-0000-0000-0000-000000000002",
                             "00000000-0000-0000-0000-000000000001");
      meta.send_notification(22,
                             "00000000-0000-0000-0000-000000000002",
                             "00000000-0000-0000-0000-000000000002");
    }
    catch (...)
    {
      std::cerr << "----------------------------------------------------------------------------------------------------" << elle::exception_string() << std::endl;
      throw;
    }
    std::cerr << "lol" << std::endl;
    reactor::wait(scope);
    std::cerr << "ByE" << std::endl;
  };
}

/*------------------.
| no_authentication |
`------------------*/

// Check what happens if a client disconnects without authenticating.

ELLE_TEST_SCHEDULED(no_authentication)
{
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    10_sec);
  {
    reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
    BOOST_CHECK_EQUAL(meta.trophonius(trophonius).size(), 0);
  }
  // Give Trophonius the opportunity to remove the unregister client (and it
  // should not).
  for (int i = 0; i < 3; ++i)
    reactor::yield();
}


/*-----------------------.
| authentication_failure |
`-----------------------*/

// Check authentication failure disconnects the user.

class MetaAuthenticationFailure:
  public Meta
{
  virtual
  void
  _register_user(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: reject user %s:%s on %s", *this, user, device, id);
    this->_response_failure(socket);
  }
};

ELLE_TEST_SCHEDULED(authentication_failure)
{
  MetaAuthenticationFailure meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
  authentify(socket);
  // Check we get a notification refusal.
  check_authentication_failure(socket);
  char c;
  BOOST_CHECK_THROW(socket.read(reactor::network::Buffer(&c, 1)),
                    reactor::network::ConnectionClosed);
}

/*------------------.
| wait_authentified |
`------------------*/

/// Check notification send immediately are delayed until the user has received
/// login confirmation.

class MetaGonzales:
  public Meta
{
  virtual
  void
  _register_user(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device) override
  {
    ELLE_LOG_SCOPE("%s: register user %s:%s on %s", *this, user, device, id);
    Client c(user, device);
    auto& trophonius = this->trophonius(id);
    BOOST_CHECK(trophonius.find(c) == trophonius.end());
    trophonius.insert(c);
    ELLE_LOG("%s: send notification before login confirmation", *this)
      this->send_notification(42, user, device);
    ELLE_LOG("%s: send login confirmation", *this)
      this->_response_success(socket);
  }
};

ELLE_TEST_SCHEDULED(wait_authentified)
{
  MetaGonzales meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  {
    reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
    authentify(socket);
    // Check the first response is the login acceptation.
    check_authentication_success(socket);
    // Check we receive the notification after.
    {
      auto json = read_json(socket);
      BOOST_CHECK_EQUAL(json["notification_type"].getInt(), 42);
    }
  }
}

/*-----------------------------------.
| notification_authentication_failed |
`-----------------------------------*/

// Check what happens if a notification is sent to a user that will be kicked
// for authentication failure.

class MetaNotificationAuthenticationFailed:
  public Meta
{
  virtual
  void
  _register_user(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: reject user %s:%s on %s", *this, user, device, id);
    this->send_notification(42, user, device);
    this->_response_failure(socket);
  }
};

// Send a notification to a non authenticated user.
ELLE_TEST_SCHEDULED(notification_authentication_failed)
{
  MetaNotificationAuthenticationFailed meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
  authentify(socket);
  // Check the first response is the login refusal.
  check_authentication_failure(socket);
}


/*-------------.
| ping_timeout |
`-------------*/

ELLE_TEST_SCHEDULED(ping_timeout)
{
  auto ping = 100_ms;
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    0,
    ping,
    300_sec);
  static auto const uuid  = "00000000-0000-0000-0000-000000000001";
  static auto const id = std::make_pair(uuid, uuid);
  auto& t = meta.trophonius(trophonius);
  BOOST_CHECK(t.find(id) == t.end());
  reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
  authentify(socket, 1, 1);
  check_authentication_success(socket);
  BOOST_CHECK(t.find(id) != t.end());
  reactor::sleep(ping * 3);
  // Read two pings
  for (int i = 0; i < 2; ++i)
  {
    auto response = read_json(socket);
    BOOST_CHECK_EQUAL(response["notification_type"].getInt(), 208);
  }
  // Chek we were disconnected.
  BOOST_CHECK_THROW(read_json(socket), reactor::network::ConnectionClosed);
  BOOST_CHECK(t.find(id) == t.end());
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(register_unregister), 0, 3);
  suite.add(BOOST_TEST_CASE(notifications), 0, 3);
  suite.add(BOOST_TEST_CASE(no_authentication), 0, 3);
  suite.add(BOOST_TEST_CASE(authentication_failure), 0, 3);
  suite.add(BOOST_TEST_CASE(wait_authentified), 0, 3);
  suite.add(BOOST_TEST_CASE(notification_authentication_failed), 0, 3);
  suite.add(BOOST_TEST_CASE(ping_timeout), 0, 3);
}
