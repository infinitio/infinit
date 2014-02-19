#include <boost/uuid/uuid_io.hpp>

#include <elle/json/json.hh>
#include <elle/log.hh>
#include <elle/test.hh>
#include <elle/utility/Move.hh>

#include <reactor/network/fingerprinted-socket.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>
#include <reactor/thread.hh>

#include <infinit/oracles/trophonius/server/Trophonius.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.server.test")

#ifdef VALGRIND
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND 0
#endif

// Local fingerprint as sha1.

static const std::vector<unsigned char> fingerprint =
{
  0xCB, 0xC5, 0x12, 0xBB, 0x86, 0x4D, 0x6B, 0x1C, 0xBC, 0x02,
  0x3D, 0xD8, 0x44, 0x75, 0xC1, 0x8C, 0x6E, 0xfC, 0x3B, 0x65
};

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
    ELLE_LOG("%s: finalize", *this);
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
        std::unique_ptr<reactor::network::Socket> socket(
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
  _response_success(reactor::network::Socket& socket)
  {
    this->response(socket,
                   std::string("{\"success\": true }"));
  }

  void
  _response_failure(reactor::network::Socket& socket)
  {
    this->response(socket,
                   std::string("{"
                               "  \"success\": false,"
                               "  \"error_code\": 0,"
                               "  \"error_details\": \"fuck you.\""
                               "}"));
  }

  void
  _serve(std::unique_ptr<reactor::network::Socket> socket)
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
            auto json_read = elle::json::read(*socket);
            auto json = boost::any_cast<elle::json::Object>(json_read);
            BOOST_CHECK(json.find("port") != json.end());
            auto port = boost::any_cast<int64_t>(json.find("port")->second);
            this->_register(*socket, id, port);
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
  _register(reactor::network::Socket& socket,
            std::string const& id,
            int port)
  {
    ELLE_LOG_SCOPE("%s: register trophonius %s on port %s", *this, id, port);
    BOOST_CHECK(this->_trophoniuses.find(id) == this->_trophoniuses.end());
    this->_trophoniuses.insert(std::make_pair(id, Trophonius(port, Clients())));
  }

  virtual
  void
  _unregister(reactor::network::Socket& socket,
              std::string const& id)
  {
    ELLE_LOG_SCOPE("%s: unregister trophonius %s", *this, id);
    BOOST_CHECK(this->_trophoniuses.find(id) != this->_trophoniuses.end());
    this->_trophoniuses.erase(id);
  }

  virtual
  void
  _register_user(reactor::network::Socket& socket,
                 std::string const& id,
                 std::string const& user,
                 std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: register user %s:%s on %s", *this, user, device, id);
    Client c(user, device);
    auto& trophonius = this->trophonius(id);
    trophonius.insert(c);
    this->_response_success(socket);
  }

  virtual
  void
  _unregister(reactor::network::Socket& socket,
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
  response(reactor::network::Socket& socket,
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
      elle::json::Object response;
      response["user_id"] = boost::lexical_cast<std::string>(user);
      response["device_id"] = boost::lexical_cast<std::string>(device);
      elle::json::Object notification;
      notification["notification_type"] = type;
      response["notification"] = notification;
      // XXX: hardcoded trophonius
      auto port = this->_trophoniuses.begin()->second.first;
      ELLE_LOG("%s: send notification: %s",
               *this,
               elle::json::pretty_print(response));
      {
        reactor::network::TCPSocket notify("127.0.0.1", port);
        elle::json::write(notify, response);
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
authentify(reactor::network::Socket& socket,
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
int
read_notification(reactor::network::Socket& socket)
{
  auto response_read = elle::json::read(socket);
  auto response = boost::any_cast<elle::json::Object>(response_read);
  auto it = response.find("notification_type");
  BOOST_CHECK(it != response.end());
  return boost::any_cast<int64_t>(it->second);
}

static
void
read_ping(reactor::network::Socket& socket)
{
  BOOST_CHECK_EQUAL(read_notification(socket), 208);
}

static
void
check_authentication_success(reactor::network::Socket& socket)
{
  auto json_read = elle::json::read(socket);
  auto json = boost::any_cast<elle::json::Object>(json_read);
  auto notification_type = json["notification_type"];
  BOOST_CHECK_EQUAL(boost::any_cast<int64_t>(notification_type), -666);
  auto response_code = json["response_code"];
  BOOST_CHECK_EQUAL(boost::any_cast<int64_t>(response_code), 200);
}

static
void
check_authentication_failure(reactor::network::Socket& socket)
{
  auto json_read = elle::json::read(socket);
  auto json = boost::any_cast<elle::json::Object>(json_read);
  auto notification_type = json["notification_type"];
  BOOST_CHECK_EQUAL(boost::any_cast<int64_t>(notification_type), -666);
  auto response_code = json["response_code"];
  BOOST_CHECK_EQUAL(boost::any_cast<int64_t>(response_code), 403);
}

static
std::unique_ptr<reactor::network::Socket>
connect_socket(bool ssl,
               infinit::oracles::trophonius::server::Trophonius& trophonius)
{
  if (ssl)
  {
    return elle::make_unique<reactor::network::FingerprintedSocket>(
      "127.0.0.1",
      boost::lexical_cast<std::string>(trophonius.port_ssl()),
      fingerprint);
  }
  else
  {
    return elle::make_unique<reactor::network::TCPSocket>(
      "127.0.0.1",
      boost::lexical_cast<std::string>(trophonius.port_tcp()));
  }
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

ELLE_TEST_SCHEDULED(notifications, (bool, ssl))
{
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    auto client = [&] (int user, int device, reactor::Barrier& b)
    {
      std::unique_ptr<reactor::network::Socket> socket(
        connect_socket(ssl, trophonius));
      authentify(*socket, user, device);
      check_authentication_success(*socket);
      b.open();
      auto notif_read = elle::json::read(*socket);
      auto notif = boost::any_cast<elle::json::Object>(notif_read);
      auto notification_type = notif["notification_type"];
      BOOST_CHECK_EQUAL(boost::any_cast<int64_t>(notification_type),
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
      ELLE_ERR("error sending notification: %s", elle::exception_string());
      throw;
    }
    reactor::wait(scope);
  };
}

/*------------------.
| no_authentication |
`------------------*/

// Check what happens if a client disconnects without authenticating.

ELLE_TEST_SCHEDULED(no_authentication, (bool, ssl))
{
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    10_sec);
  {
    std::unique_ptr<reactor::network::Socket> socket(
        connect_socket(ssl, trophonius));
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
  _register_user(reactor::network::Socket& socket,
                 std::string const& id,
                 std::string const& user,
                 std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: reject user %s:%s on %s", *this, user, device, id);
    this->_response_failure(socket);
  }
};

ELLE_TEST_SCHEDULED(authentication_failure, (bool, ssl))
{
  MetaAuthenticationFailure meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  std::unique_ptr<reactor::network::Socket> socket(
        connect_socket(ssl, trophonius));
  authentify(*socket);
  // Check we get a notification refusal.
  check_authentication_failure(*socket);
  char c;
  BOOST_CHECK_THROW(socket->read(reactor::network::Buffer(&c, 1)),
                    reactor::network::ConnectionClosed);
}

/*------------------.
| wait_authentified |
`------------------*/

/// Check notification sent immediately are delayed until the user has received
/// login confirmation.

class MetaGonzales:
  public Meta
{
  virtual
  void
  _register_user(reactor::network::Socket& socket,
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

ELLE_TEST_SCHEDULED(wait_authentified, (bool, ssl))
{
  MetaGonzales meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  {
    std::unique_ptr<reactor::network::Socket> socket(
        connect_socket(ssl, trophonius));
    authentify(*socket);
    // Check the first response is the login acceptation.
    check_authentication_success(*socket);
    // Check we receive the notification after.
    {
      auto json_read = elle::json::read(*socket);
      auto json = boost::any_cast<elle::json::Object>(json_read);
      auto notification_type = json["notification_type"];
      BOOST_CHECK_EQUAL(boost::any_cast<int64_t>(notification_type), 42);
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
  _register_user(reactor::network::Socket& socket,
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
ELLE_TEST_SCHEDULED(notification_authentication_failed, (bool, ssl))
{
  MetaNotificationAuthenticationFailed meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    60_sec,
    300_sec);
  std::unique_ptr<reactor::network::Socket> socket(
        connect_socket(ssl, trophonius));
  authentify(*socket);
  // Check the first response is the login refusal.
  check_authentication_failure(*socket);
}


/*-------------.
| ping_timeout |
`-------------*/

ELLE_TEST_SCHEDULED(ping_timeout, (bool, ssl))
{
  auto ping = 100_ms;
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
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
  std::unique_ptr<reactor::network::Socket> socket(
        connect_socket(ssl, trophonius));
  authentify(*socket, 1, 1);
  check_authentication_success(*socket);
  BOOST_CHECK(t.find(id) != t.end());
  reactor::sleep(ping * 3);
  // Read some pings until the conncetion is closed. The number of pings will
  // be determined by timing but will be three or four.
  bool connection_closed = false;
  for (int i = 0; i < 4; i++)
  {
    try
    {
      read_ping(*socket);
    }
    catch (reactor::network::ConnectionClosed const& exception)
    {
      connection_closed = true;
      break;
    }
  }
  // Check we were disconnected.
  BOOST_CHECK(connection_closed);
  BOOST_CHECK(t.find(id) == t.end());
}

/*--------.
| replace |
`--------*/

// Send a notification to a non authenticated user.
ELLE_TEST_SCHEDULED(replace, (bool, ssl))
{
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    30_sec,
    300_sec);
  static auto const uuid  = "00000000-0000-0000-0000-000000000001";
  static auto const id = std::make_pair(uuid, uuid);
  auto& t = meta.trophonius(trophonius);
  BOOST_CHECK(t.find(id) == t.end());
  {
    ELLE_LOG("connect the first client");
    std::unique_ptr<reactor::network::Socket> socket1(
        connect_socket(ssl, trophonius));
    authentify(*socket1, 1, 1);
    check_authentication_success(*socket1);
    BOOST_CHECK(t.find(id) != t.end());
    meta.send_notification(1,
                           "00000000-0000-0000-0000-000000000001",
                           "00000000-0000-0000-0000-000000000001");
    ELLE_LOG("read notification from the first client")
      BOOST_CHECK_EQUAL(read_notification(*socket1), 1);
    ELLE_LOG("connect a replacement client");
    std::unique_ptr<reactor::network::Socket> socket2(
        connect_socket(ssl, trophonius));
    authentify(*socket2, 1, 1);
    check_authentication_success(*socket2);
    BOOST_CHECK(t.find(id) != t.end());
    meta.send_notification(2,
                           "00000000-0000-0000-0000-000000000001",
                           "00000000-0000-0000-0000-000000000001");
    // Check the first socket was disconnected.
    ELLE_LOG("check the old client is disconnected")
      BOOST_CHECK_THROW(elle::json::read(*socket1),
                        reactor::network::ConnectionClosed);
    ELLE_LOG("read notification from the new client")
      BOOST_CHECK_EQUAL(read_notification(*socket2), 2);
  }
  reactor::sleep(100_ms); // XXX: wait for tropho to disconnect it.
  BOOST_CHECK(t.find(id) == t.end());
}

/*------------------.
| Bad SSL handshake |
`------------------*/

ELLE_TEST_SCHEDULED(bad_ssl_handshake)
{
  Meta meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    0,
    "localhost",
    meta.port(),
    0,
    30_sec,
    30_sec);

  reactor::network::TCPSocket socket(
    "127.0.0.1",
    boost::lexical_cast<std::string>(trophonius.port_ssl()));
  socket.write(elle::ConstWeakBuffer("pwal\n"));
  BOOST_CHECK_THROW(socket.get(), reactor::network::ConnectionClosed);
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 15 : 3;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(register_unregister), 0, timeout);

  auto notifications_tcp = std::bind(notifications, false);
  suite.add(BOOST_TEST_CASE(notifications_tcp), 0, timeout);
  auto notifications_ssl = std::bind(notifications, true);
  suite.add(BOOST_TEST_CASE(notifications_ssl), 0, timeout);

  auto no_authentication_tcp = std::bind(no_authentication, false);
  suite.add(BOOST_TEST_CASE(no_authentication_tcp), 0, timeout);
  auto no_authentication_ssl = std::bind(no_authentication, true);
  suite.add(BOOST_TEST_CASE(no_authentication_ssl), 0, timeout);

  auto authentication_failure_tcp = std::bind(authentication_failure, false);
  suite.add(BOOST_TEST_CASE(authentication_failure_tcp), 0, timeout);
  auto authentication_failure_ssl = std::bind(authentication_failure, true);
  suite.add(BOOST_TEST_CASE(authentication_failure_ssl), 0, timeout);

  auto wait_authentified_tcp = std::bind(wait_authentified, false);
  suite.add(BOOST_TEST_CASE(wait_authentified_tcp), 0, timeout);
  auto wait_authentified_ssl = std::bind(wait_authentified, true);
  suite.add(BOOST_TEST_CASE(wait_authentified_ssl), 0, timeout);

  auto notification_authentication_failed_tcp =
    std::bind(notification_authentication_failed, false);
  suite.add(BOOST_TEST_CASE(notification_authentication_failed_tcp), 0, timeout);
  auto notification_authentication_failed_ssl =
    std::bind(notification_authentication_failed, true);
  suite.add(BOOST_TEST_CASE(notification_authentication_failed_ssl), 0, timeout);

  auto ping_timeout_tcp = std::bind(ping_timeout, false);
  suite.add(BOOST_TEST_CASE(ping_timeout_tcp), 0, timeout);
  auto ping_timeout_ssl = std::bind(ping_timeout, true);
  suite.add(BOOST_TEST_CASE(ping_timeout_ssl), 0, timeout);

  auto replace_tcp = std::bind(replace, false);
  suite.add(BOOST_TEST_CASE(replace_tcp), 0, timeout);
  auto replace_ssl = std::bind(replace, true);
  suite.add(BOOST_TEST_CASE(replace_ssl), 0, timeout);

  suite.add(BOOST_TEST_CASE(bad_ssl_handshake), 0, timeout);
}
