#include <boost/uuid/uuid_io.hpp>

#include <elle/json/json.hh>
#include <elle/log.hh>
#include <elle/test.hh>
#include <elle/utility/Move.hh>

#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/fingerprinted-socket.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>
#include <reactor/thread.hh>

#include <infinit/oracles/apertus/Apertus.hh>

ELLE_LOG_COMPONENT("infinit.oracles.apertus.server.test")

#ifdef VALGRIND
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND 0
#endif

static const std::vector<unsigned char> fingerprint =
{
  0x98, 0x55, 0xEF, 0x72, 0x1D, 0xFC, 0x1B, 0xF5, 0xEA, 0xF5,
  0x35, 0xC5, 0xF9, 0x32, 0x85, 0x38, 0x38, 0x2C, 0xCA, 0x91
};

class Meta
{
public:
  Meta():
    _server(),
    _port(0),
    _bandwidth_update_count(0),
    _accepter()
  {
    this->_server.listen(0);
    this->_port = this->_server.port();
    ELLE_LOG("%s: listen on port %s", *this, this->_port);
    this->_accepter.reset(
      new reactor::Thread(*reactor::Scheduler::scheduler(),
                          "accepter",
                          std::bind(&Meta::_accept, std::ref(*this))));
  }

  ~Meta()
  {
    this->_accepter->terminate_now();
    ELLE_LOG("%s: finalize", *this);
  }

  typedef std::string Apertus;
  typedef std::unordered_map<std::string, Apertus> Apertuses;
  ELLE_ATTRIBUTE(reactor::network::TCPServer, server);
  ELLE_ATTRIBUTE_R(int, port);
  ELLE_ATTRIBUTE_R(int, bandwidth_update_count);
  ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter);
  ELLE_ATTRIBUTE_R(Apertuses, apertuses);
  ELLE_ATTRIBUTE_RX(reactor::Barrier, apertus_registered);
  ELLE_ATTRIBUTE_RX(reactor::Signal, apertus_unregistered);
  ELLE_ATTRIBUTE_RX(reactor::Signal, apertus_bandwidth_updated);

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
        BOOST_CHECK_EQUAL(chunks[1], "apertus");
        std::string id = chunks[2];
        if (method == "PUT")
        {
          auto json_read = elle::json::read(*socket);
          auto json = boost::any_cast<elle::json::Object>(json_read);
          BOOST_CHECK(json.find("port_ssl") != json.end());
          BOOST_CHECK(json.find("port_tcp") != json.end());
          auto port_ssl = json.find("port_ssl")->second;
          auto port_tcp = json.find("port_tcp")->second;
          this->_register(*socket, id,
            boost::any_cast<int64_t>(port_ssl), boost::any_cast<int64_t>(port_tcp));
        }
        else if (method == "DELETE")
        {
          this->_unregister(*socket, id);
        }
        else if (method == "POST")
        {
          auto json_read = elle::json::read(*socket);
          auto json = boost::any_cast<elle::json::Object>(json_read);
          BOOST_CHECK(json.find("bandwidth") != json.end());
          BOOST_CHECK(json.find("number_of_transfers") != json.end());
          auto bandwidth = json.find("bandwidth")->second;
          auto number_of_transfers = json.find("number_of_transfers")->second;
          this->_update_bandwidth(*socket, id,
            boost::any_cast<int64_t>(bandwidth),
            boost::any_cast<int64_t>(number_of_transfers));
        }
        this->response(*socket,
                       std::string("{\"success\": true }"));
        return;
      }
    }
  }

  virtual
  void
  _register(reactor::network::Socket& socket,
            std::string const& id,
            int port_ssl,
            int port_tcp)
  {
    ELLE_LOG_SCOPE("%s: register apertus %s on SSL port %s and TCP port %s",
                   *this, id, port_ssl, port_tcp);
    BOOST_CHECK(this->_apertuses.find(id) == this->_apertuses.end());
    this->_apertuses.insert(std::make_pair(id, Apertus()));
    this->_apertus_registered.open();
  }

  virtual
  void
  _unregister(reactor::network::Socket& socket,
              std::string const& id)
  {
    ELLE_LOG_SCOPE("%s: unregister apertus %s", *this, id);
    BOOST_CHECK(this->_apertuses.find(id) != this->_apertuses.end());
    this->_apertuses.erase(id);
    this->_apertus_unregistered.signal();
  }

  virtual
  void
  _update_bandwidth(reactor::network::Socket& socket,
                    std::string const& id,
                    int bandwidth,
                    int number_of_transfers)
  {
    ELLE_LOG_SCOPE("%s: update apertus bandwidth %s", *this, id);
    this->_bandwidth_update_count++;
    this->_apertus_bandwidth_updated.signal();
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

};

/*--------------------.
| register_unregister |
`--------------------*/

// Test registering and unregistering an Apertus from meta.

ELLE_TEST_SCHEDULED(register_unregister)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  {
    std::unique_ptr<infinit::oracles::apertus::Apertus> apertus;

    apertus.reset(
      new infinit::oracles::apertus::Apertus(
        "localhost",
        meta.port(),
        "localhost",
        0,
        0,
        1000_sec));

    reactor::wait(meta.apertus_registered());
    ELLE_LOG("registered apertus");
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 1);

    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      scope.run_background("check unregistered", [&]
      {
        reactor::wait(meta.apertus_unregistered());
        ELLE_LOG("unregistered apertus");
      });
      scope.run_background("stop", [&]
      {
        apertus->stop();
      });
      scope.wait();
    };
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  }
}

/*---------------------.
| no_update_after_stop |
`---------------------*/

// Check that Apertus unregisters itself and doesn't send monitor updates on
// a stop call.

ELLE_TEST_SCHEDULED(no_update_after_stop)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  {
    auto tick_rate = 1_sec;
    std::unique_ptr<infinit::oracles::apertus::Apertus> apertus;
    apertus.reset(
      new infinit::oracles::apertus::Apertus(
        "localhost",
        meta.port(),
        "localhost",
        0,
        0,
        tick_rate));

    reactor::wait(meta.apertus_registered());
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 1);
    BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 0);
    reactor::wait(meta.apertus_bandwidth_updated());
    BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 1);

    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      scope.run_background("check unregistered", [&]
      {
        reactor::wait(meta.apertus_unregistered());
      });
      scope.run_background("stop", [&]
      {
        apertus->stop();
      });
      scope.wait();
    };
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
    reactor::sleep(tick_rate * 2);
    BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 1);
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  }
}

/*----------------.
| simple_transfer |
`----------------*/

// Check that two clients can transfer data.

ELLE_TEST_SCHEDULED(simple_transfer)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  {
    auto tick_rate = 1_sec;
    std::unique_ptr<infinit::oracles::apertus::Apertus> apertus;
    apertus.reset(
      new infinit::oracles::apertus::Apertus(
        "localhost",
        meta.port(),
        "localhost",
        0,
        0,
        tick_rate));

    reactor::wait(meta.apertus_registered());
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 1);

    BOOST_CHECK_EQUAL(apertus->workers().size(), 0);

    std::string passphrase(32, 'b');
    reactor::network::FingerprintedSocket socket1(
      "127.0.0.1",
      boost::lexical_cast<std::string>(apertus->port_ssl()),
      fingerprint);
    socket1.write(elle::ConstWeakBuffer(elle::sprintf(" %s", passphrase)));

    reactor::network::FingerprintedSocket socket2(
      "127.0.0.1",
      boost::lexical_cast<std::string>(apertus->port_ssl()),
      fingerprint);
    socket2.write(elle::ConstWeakBuffer(elle::sprintf(" %s", passphrase)));

    reactor::wait(meta.apertus_bandwidth_updated());
    BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 1);
    BOOST_CHECK_EQUAL(apertus->workers().size(), 1);

    static std::string const some_stuff = std::string(10, 'a') +
      std::string("\n");
    socket1.write(some_stuff);
    BOOST_CHECK_EQUAL(socket2.read_until(some_stuff), some_stuff);
  }
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
}

/*-----------------.
| ssl_tcp_transfer |
`-----------------*/

// Check that two clients (one SSL, one TCP) can transfer data.

ELLE_TEST_SCHEDULED(ssl_tcp_transfer)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  {
    auto tick_rate = 1_sec;
    std::unique_ptr<infinit::oracles::apertus::Apertus> apertus;
    apertus.reset(
      new infinit::oracles::apertus::Apertus(
        "localhost",
        meta.port(),
        "localhost",
        0,
        0,
        tick_rate));

    reactor::wait(meta.apertus_registered());
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 1);

    BOOST_CHECK_EQUAL(apertus->workers().size(), 0);

    std::string passphrase(32, 'b');
    reactor::network::FingerprintedSocket socket1(
      "127.0.0.1",
      boost::lexical_cast<std::string>(apertus->port_ssl()),
      fingerprint);
    socket1.write(elle::ConstWeakBuffer(elle::sprintf(" %s", passphrase)));

    reactor::network::TCPSocket socket2(
      "127.0.0.1",
      boost::lexical_cast<std::string>(apertus->port_tcp()));
    socket2.write(elle::ConstWeakBuffer(elle::sprintf(" %s", passphrase)));

    reactor::wait(meta.apertus_bandwidth_updated());
    BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 1);
    BOOST_CHECK_EQUAL(apertus->workers().size(), 1);

    static std::string const some_stuff = std::string(10, 'a') +
      std::string("\n");
    socket1.write(some_stuff);
    BOOST_CHECK_EQUAL(socket2.read_until(some_stuff), some_stuff);
  }
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
}

/*-------------------.
| wait_for_transfers |
`-------------------*/

// Check that when Apertus receives the stop command, it unregisters but waits
// for the current transfers to finish.

ELLE_TEST_SCHEDULED(wait_for_transfers)
{
  Meta meta;
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
  {
    auto tick_rate = 1_sec;
    std::unique_ptr<infinit::oracles::apertus::Apertus> apertus;
    apertus.reset(
      new infinit::oracles::apertus::Apertus(
        "localhost",
        meta.port(),
        "localhost",
        0,
        0,
        tick_rate));

    reactor::wait(meta.apertus_registered());
    BOOST_CHECK_EQUAL(meta.apertuses().size(), 1);

    BOOST_CHECK_EQUAL(apertus->workers().size(), 0);

    std::string passphrase(32, 'o');
    reactor::network::FingerprintedSocket socket1(
      "127.0.0.1",
      boost::lexical_cast<std::string>(apertus->port_ssl()),
      fingerprint);
    socket1.write(elle::ConstWeakBuffer(elle::sprintf(" %s", passphrase)));

    reactor::network::FingerprintedSocket socket2(
      "127.0.0.1",
      boost::lexical_cast<std::string>(apertus->port_ssl()),
      fingerprint);
    socket2.write(elle::ConstWeakBuffer(elle::sprintf(" %s", passphrase)));

    reactor::wait(meta.apertus_bandwidth_updated());
    BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 1);
    BOOST_CHECK_EQUAL(apertus->workers().size(), 1);

    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      scope.run_background("check unregistered", [&]
      {
        reactor::wait(meta.apertus_unregistered());
        reactor::sleep(tick_rate * 2);
        BOOST_CHECK_EQUAL(meta.bandwidth_update_count(), 1);
        static std::string const some_stuff("some stuffs\n");
        socket1.write(some_stuff);
        BOOST_CHECK_EQUAL(socket2.read_until(some_stuff), some_stuff);
        socket1.close();
        socket2.close();
      });
      scope.run_background("stop", [&]
      {
        BOOST_CHECK_EQUAL(apertus->workers().size(), 1);
        apertus->stop();
        BOOST_CHECK_EQUAL(apertus->workers().size(), 0);
      });
      scope.wait();
    };
  }
  BOOST_CHECK_EQUAL(meta.apertuses().size(), 0);
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 20 : 5;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(register_unregister), 0, timeout);
  suite.add(BOOST_TEST_CASE(no_update_after_stop), 0, timeout);
  suite.add(BOOST_TEST_CASE(simple_transfer), 0, timeout);
  suite.add(BOOST_TEST_CASE(ssl_tcp_transfer), 0, timeout);
  suite.add(BOOST_TEST_CASE(wait_for_transfers), 0, timeout);
}