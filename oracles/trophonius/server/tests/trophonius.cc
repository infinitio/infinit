#include <boost/uuid/uuid_io.hpp>

#include <elle/log.hh>
#include <elle/test.hh>
#include <elle/utility/Move.hh>

#include <reactor/Scope.hh>
#include <reactor/scheduler.hh>

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

  ELLE_ATTRIBUTE(reactor::network::TCPServer, server);
  ELLE_ATTRIBUTE_R(int, port);
  ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter);
  typedef std::pair<std::string, std::string> Client;
  typedef std::unordered_set<Client, boost::hash<Client>> Clients;
  typedef std::unordered_map<std::string, Clients> Trophoniuses;
  ELLE_ATTRIBUTE_R(Trophoniuses, trophoniuses);

  Clients&
  trophonius(std::string const& id)
  {
    BOOST_CHECK(this->_trophoniuses.find(id) != this->_trophoniuses.end());
    return this->_trophoniuses.find(id)->second;
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
            this->_register(*socket, id);
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
          this->_register(*socket, id, user, device);
        else if (method == "DELETE")
          this->_unregister(*socket, id, user, device);
        this->response(*socket,
                       std::string("{\"success\": true }"));
        return;
      }
    }
  }

  virtual
  void
  _register(reactor::network::TCPSocket& socket,
            std::string const& id)
  {
    ELLE_LOG_SCOPE("%s: register trophonius %s", *this, id);
    BOOST_CHECK(this->_trophoniuses.find(id) == this->_trophoniuses.end());
    this->_trophoniuses.insert(std::make_pair(id, Clients()));
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
  _register(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device)
  {
    ELLE_LOG_SCOPE("%s: register user %s:%s on %s", *this, user, device, id);
    Client c(user, device);
    auto& trophonius = this->trophonius(id);
    BOOST_CHECK(trophonius.find(c) == trophonius.end());
    trophonius.insert(c);
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
};

static
std::ostream&
operator << (std::ostream& s, Meta const& meta)
{
  s << "Meta(" << meta.port() << ")";
  return s;
}

ELLE_TEST_SCHEDULED(register_unregister)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.trophoniuses().size(), 0);
  {
    infinit::oracles::trophonius::server::Trophonius trophonius(
      0,
      "localhost",
      meta.port(),
      0,
      60_sec,
      300_sec);
    BOOST_CHECK_EQUAL(meta.trophoniuses().size(), 1);
  }
  BOOST_CHECK_EQUAL(meta.trophoniuses().size(), 0);
}

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

class MetaGonzales:
  public Meta
{
  virtual
  void
  _register(reactor::network::TCPSocket& socket,
            std::string const& id,
            std::string const& user,
            std::string const& device)
  {
    Meta::_register(socket, id, user, device);
    json_spirit::Object response;
    response["user_id"] = boost::lexical_cast<std::string>(user);
    response["device_id"] = boost::lexical_cast<std::string>(device);
    json_spirit::Object notification;
    notification["notification_type"] = 42;
    response["notification"] = notification;
    // XXX: hardcoded port
    reactor::network::TCPSocket notify("127.0.0.1", 8080);
    write_json(notify, response);
  }
};

ELLE_TEST_SCHEDULED(wait_authentified)
{
  MetaGonzales meta;
  infinit::oracles::trophonius::server::Trophonius trophonius(
    0,
    "localhost",
    meta.port(),
    8080, // XXX: hardcoded port
    60_sec,
    300_sec);
  reactor::network::TCPSocket socket("127.0.0.1", trophonius.port());
  static std::string const auth =
    "{"
    "  \"device_id\": \"00000000-0000-0000-0000-000000000000\","
    "  \"session_id\": \"00000000-0000-0000-0000-000000000000\","
    "  \"user_id\": \"00000000-0000-0000-0000-000000000000\""
    "}\n";
  socket.write(auth);
  // Check the first response is the login acceptation.
  {
    auto json = read_json(socket);
    BOOST_CHECK_EQUAL(json["notification_type"].getInt(), -666);
  }
  // Check we receive the notification after.
  {
    auto json = read_json(socket);
    BOOST_CHECK_EQUAL(json["notification_type"].getInt(), 42);
  }
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(no_authentication));
  suite.add(BOOST_TEST_CASE(register_unregister));
  suite.add(BOOST_TEST_CASE(wait_authentified));
}
