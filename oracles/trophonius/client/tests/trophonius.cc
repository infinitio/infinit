#include <elle/json/json.hh>
#include <elle/log.hh>
#include <elle/test.hh>
#include <elle/utility/Move.hh>

#include <infinit/oracles/trophonius/Client.hh>

#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>
#include <reactor/signal.hh>
#include <reactor/Scope.hh>
#include <reactor/thread.hh>

ELLE_LOG_COMPONENT("infinit.oracles.trophonius.client.test")

#ifdef VALGRIND
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND 0
#endif

typedef enum __NotificationCode
{
  NONE_NOTIFICATION = 0,
  USER_STATUS_NOTIFICATION = 8,
  TRANSACTION_NOTIFICATION = 7,
  NEW_SWAGGER_NOTIFICATION = 9,
  PEER_CONNECTION_UPDATE_NOTIFICATION = 11,
  NETWORK_UPDATE_NOTIFICATION = 128,
  MESSAGE_NOTIFICATION = 217,
  PING_NOTIFICATION = 208,
  CONNECTION_ENABLED_NOTIFICATION = -666,
  SUICIDE_NOTIFICATION = 666,
}
NotificationCode;

class Trophonius
{
public:
  Trophonius():
    _server(),
    _port(),
    _accepter()
  {
    this->_server.listen(0);
    this->_port = this->_server.port();
    ELLE_LOG("%s: listen on port %s", *this, this->_port);
    this->_accepter.reset(
      new reactor::Thread(*reactor::Scheduler::scheduler(),
                          "accepter",
                          std::bind(&Trophonius::_accept,
                                    std::ref(*this))));
  }

  ~Trophonius()
  {
    this->_accepter->terminate_now();
    ELLE_LOG("%s: finalize", *this);
  }

  ELLE_ATTRIBUTE_RX(reactor::Signal, poked);
  ELLE_ATTRIBUTE_RX(reactor::Signal, client_connected);

  ELLE_ATTRIBUTE(reactor::network::TCPServer, server);
  ELLE_ATTRIBUTE_R(int, port);
  ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, accepter);

protected:
  reactor::network::TCPServer&
  server()
  {
    return this->_server;
  }

  virtual
  void
  _accept()
  {
    while (true)
    {
      std::unique_ptr<reactor::network::TCPSocket> socket(
        this->_server.accept());
      ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
      auto poke_read = elle::json::read(*socket);
      auto poke = boost::any_cast<elle::json::Object>(poke_read);
      elle::json::write(*socket, poke);
      ELLE_LOG("%s replied to poke", *this);
      this->_serve(*socket);
    }
  }

  virtual
  void
  _serve(reactor::network::TCPSocket& socket) = 0;

  void
  _send_notification(reactor::network::TCPSocket& socket,
                     std::string const& message)
  {
    elle::json::Object notification;
    notification["notification_type"] = int(MESSAGE_NOTIFICATION);
    notification["sender_id"] = std::string("id");
    notification["message"] = message;
    ELLE_LOG("%s: write: %s",
             *this,
             elle::json::pretty_print(notification));
    elle::json::write(socket, notification);
  }

  virtual
  void
  _login_response(reactor::network::TCPSocket& socket)
  {
    this->_login_response(socket, true);
  }

  void
  _login_response(reactor::network::TCPSocket& socket, bool success)
  {
    elle::json::Object login_response;
    login_response["notification_type"] = int(CONNECTION_ENABLED_NOTIFICATION);
    login_response["response_code"] = success ? int(200) : int(500);
    login_response["response_details"] = std::string("nothing");
    ELLE_LOG("%s: login response: %s",
             *this,
             elle::json::pretty_print(login_response));
    elle::json::write(socket, login_response);
  }
};

/*-----.
| Poke |
`-----*/

// Check that the client's poke reports connectivity correctly.

class PokeTrophonius:
  public Trophonius
{
public:
  PokeTrophonius():
    Trophonius(),
    _round(0)
  {}

  static int const total_rounds = 6;
  ELLE_ATTRIBUTE_RW(int, round);

protected:
  virtual
  void
  _accept()
  {
    if (this->round() == 0) // Normal case.
    {
      std::unique_ptr<reactor::network::TCPSocket> socket(
        this->server().accept());
      ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
      auto poke_read = elle::json::read(*socket);
      auto poke = boost::any_cast<elle::json::Object>(poke_read);
      elle::json::write(*socket, poke);
      ELLE_LOG("%s replied to poke", *this);
      reactor::wait(this->poked());
    }
    else if (this->round() == 1) // Connection refused.
    {
      // Do nothing.
      reactor::wait(this->poked());
    }
    else if (this->round() == 2) // No reply.
    {
      std::unique_ptr<reactor::network::TCPSocket> socket(
        this->server().accept());
      ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
      auto poke_read = elle::json::read(*socket);
      reactor::wait(this->poked());
    }
    else if (this->round() == 3) // Unable to resolve.
    {
      // Do nothing.
      reactor::wait(this->poked());
    }
    else if (this->round() == 4) // Wrong JSON reply.
    {
      std::unique_ptr<reactor::network::TCPSocket> socket(
        this->server().accept());
      ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
      auto poke_read = elle::json::read(*socket);
      elle::json::Object rubbish;
      rubbish["poke"] = std::string("rubbish");
      elle::json::write(*socket, rubbish);
      ELLE_LOG("%s replied incorrect JSON to poke", *this);
      reactor::wait(this->poked());
    }
    else if (this->round() == 5) // HTML reply.
    {
      std::unique_ptr<reactor::network::TCPSocket> socket(
        this->server().accept());
      ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
      auto poke_read = elle::json::read(*socket);
      std::string rubbish("<h1>Some randome HTML</h1>\n<p>shouldn't break anything</p>");
      socket->write(rubbish);
      ELLE_LOG("%s replied HTML to poke", *this);
      reactor::wait(this->poked());
    }
  }

  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    // Do nothing.
  }

};

ELLE_TEST_SCHEDULED(poke)
{
  PokeTrophonius tropho;
  {
    using namespace infinit::oracles::trophonius;
    std::unique_ptr<Client> client;
    for (int round = 0; round < tropho.total_rounds; round++)
    {
      client.reset(new Client(
        "127.0.0.1",
        tropho.port(),
        [] (bool) {}, // connect callback
        [] (void) {})  // reconnection failed callback
      );
      ELLE_LOG("round: %s", round);
      tropho.round(round);
      BOOST_CHECK_EQUAL(tropho.round(), round);
      if (round == 0) // Normal case.
      {
        ELLE_LOG("normal case");
        BOOST_CHECK_EQUAL(client->poke(), true);
        tropho.poked().signal();
      }
      else if (round == 1) // Connection refused.
      {
        ELLE_LOG("connection refused");
        BOOST_CHECK_EQUAL(client->poke(), false);
        tropho.poked().signal();
      }
      else if (round == 2) // No reply.
      {
        ELLE_LOG("no reply");
        BOOST_CHECK_EQUAL(client->poke(), false);
        tropho.poked().signal();
      }
      else if (round == 3) // Unable to resolve.
      {
        ELLE_LOG("unable to resolve");
        client.reset(new Client(
          "does.not.exist",
          tropho.port(),
          [] (bool) {}, // connect callback
          [] (void) {})  // reconnection failed callback
        );
        BOOST_CHECK_EQUAL(client->poke(), false);
        tropho.poked().signal();
      }
      else if (round == 4) // Wrong JSON reply.
      {
        ELLE_LOG("wrong json reply");
        BOOST_CHECK_EQUAL(client->poke(), false);
        tropho.poked().signal();
      }
      else if (round == 5) // HTML response
      {
        ELLE_LOG("wrong json reply");
        BOOST_CHECK_EQUAL(client->poke(), false);
        tropho.poked().signal();
      }
    }
  }
}

/*-------------.
| Notification |
`-------------*/

// Check that a client can receive a simple notification message and will
// reconnectionn when the socket is closed.

class NotificationTrophonius:
  public Trophonius
{
protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    ELLE_LOG("serve notification test");
    reactor::wait(this->poked());
    this->_login_response(socket);
    reactor::wait(this->client_connected());
    this->_send_notification(socket, "hello");
  }
};

ELLE_TEST_SCHEDULED(notification)
{
  static int const reconnections = 5;
  NotificationTrophonius tropho;
  {
    using namespace infinit::oracles::trophonius;
    std::unique_ptr<Client> client;
    client.reset(new Client(
      "127.0.0.1",
      tropho.port(),
      [] (bool) {}, // connect callback
      [] (void) {})  // reconnection failed callback
    );
    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      // We need to poke the server before connecting so that we have a valid
      // socket. After that the fake server is controlled by signalling the end
      // of pokes and connections.
      scope.run_background("notification check", [&]
      {
        reactor::wait(tropho.poked());
        client->connect("0", "0", "0");
        reactor::wait(client->connected());
        tropho.client_connected().signal();
        for (int i = 0; i < reconnections; ++i)
        {
          ELLE_LOG("poll notifications");
          std::unique_ptr<Notification> notification = client->poll();
          ELLE_DEBUG("back from poll");
          BOOST_CHECK_EQUAL(client->reconnected(), i);
          BOOST_CHECK(notification);
          ELLE_LOG("got notification type: %s", notification->notification_type);
          BOOST_CHECK_EQUAL(notification->notification_type,
                            NotificationType::message);
          // Wait for us to be connected before running test again.
          // This is done by forwarding the client's signals to our fake server.
          reactor::wait(client->poked());
          tropho.poked().signal();
          reactor::wait(client->connected());
          tropho.client_connected().signal();
        }
      });
      scope.run_background("poke server", [&]
      {
        BOOST_CHECK_EQUAL(client->poke(), true);
        tropho.poked().signal();
      });
      scope.wait();
    };
  }
}


/*-----.
| Ping |
`-----*/

// Check the client pings every period.

class PingTrophonius:
  public Trophonius
{
public:
  PingTrophonius(boost::posix_time::time_duration const& period):
    Trophonius(),
    _period(period),
    _ping_received(0)
  {}

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    reactor::wait(this->client_connected());
    while (true)
    {
      auto& sched = *reactor::Scheduler::scheduler();
      // Ping the client to keep it happy.
      std::unique_ptr<reactor::Thread> ping(sched.every([&]
        {
          elle::json::Object msg;
          msg["notification_type"] = int(PING_NOTIFICATION);
          ELLE_LOG("send ping");
          elle::json::write(socket, msg);
        },
        "ping", this->_period)
      );
      elle::With<elle::Finally>([&] { ping->terminate_now(); }) << [&]
      {
        // Expect a ping roughly every period
        auto previous = boost::posix_time::microsec_clock::local_time();
        while (true)
        {
          std::string buf(512, '\0');
          size_t bytes = socket.read_some(reactor::network::Buffer(buf));
          buf.resize(bytes);
          if (buf[buf.length() - 1] != '\n')
            continue;
          auto now = boost::posix_time::microsec_clock::local_time();
          auto diff = now - previous;
          ELLE_LOG("got ping after %s", diff);
          BOOST_CHECK_LT(diff, this->_period * 11 / 10);
          previous = now;
          ++this->_ping_received;;
        }
      };
    }
  }

private:
  boost::posix_time::time_duration _period;
  ELLE_ATTRIBUTE_R(int, ping_received);
};

ELLE_TEST_SCHEDULED(ping)
{
  boost::posix_time::time_duration const period = 100_ms;
  boost::posix_time::time_duration const run_time = 3_sec;
  int periods = run_time.total_milliseconds() / period.total_milliseconds();

  PingTrophonius tropho(period);

  using namespace infinit::oracles::trophonius;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    std::unique_ptr<Client> client;
    client.reset(new Client(
      "127.0.0.1",
      tropho.port(),
      [] (bool) {}, // connect callback
      [] (void) {})  // reconnection failed callback
    );
    scope.run_background("initial poke", [&]
    {
      BOOST_CHECK(client->poke());
      tropho.poked().signal();
    });
    scope.run_background("client", [&]
    {
      reactor::wait(tropho.poked());
      client->ping_period(period);
      client->connect("0", "0", "0");
      reactor::wait(client->connected());
      tropho.client_connected().signal();
      elle::SafeFinally check([&]
      {
        BOOST_CHECK_EQUAL(client->reconnected(), 0);
        BOOST_CHECK_LE(std::abs(tropho.ping_received() - periods), 1);
      });
      while (true)
      {
        ELLE_LOG("poll notification");
        std::unique_ptr<Notification> notification = client->poll();
      }
    });
    scope.wait(run_time);
  };
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 15 : 5;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(poke), 0, 6 * timeout);
  suite.add(BOOST_TEST_CASE(notification), 0, timeout);
  suite.add(BOOST_TEST_CASE(ping), 0, timeout);
}