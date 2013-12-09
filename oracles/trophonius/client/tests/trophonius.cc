#include <elle/cast.hh>
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
    try
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
    catch (reactor::network::ConnectionClosed const&)
    {
      ELLE_LOG("%s: ignore connection closed on killing accepter", *this);
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
    reactor::Duration poke_timeout = 200_ms;
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
        BOOST_CHECK_EQUAL(client->poke(poke_timeout), true);
        tropho.poked().signal();
      }
      else if (round == 1) // Connection refused.
      {
        ELLE_LOG("connection refused");
        BOOST_CHECK_EQUAL(client->poke(poke_timeout), false);
        tropho.poked().signal();
      }
      else if (round == 2) // No reply.
      {
        ELLE_LOG("no reply");
        BOOST_CHECK_EQUAL(client->poke(poke_timeout), false);
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
        BOOST_CHECK_EQUAL(client->poke(poke_timeout), false);
        tropho.poked().signal();
      }
      else if (round == 4) // Wrong JSON reply.
      {
        ELLE_LOG("wrong json reply");
        BOOST_CHECK_EQUAL(client->poke(poke_timeout), false);
        tropho.poked().signal();
      }
      else if (round == 5) // HTML response
      {
        ELLE_LOG("wrong json reply");
        BOOST_CHECK_EQUAL(client->poke(poke_timeout), false);
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
    auto connect_data = elle::json::read(socket);
    auto connect_msg = boost::any_cast<elle::json::Object>(connect_data);
    ELLE_LOG("%s: got connect message: %s",
             *this,
             elle::json::pretty_print(connect_msg));
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
    _ping_received(0),
    _period(period)
  {}

ELLE_ATTRIBUTE_R(int, ping_received);

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    reactor::wait(this->poked());
    auto connect_data = elle::json::read(socket);
    auto connect_msg = boost::any_cast<elle::json::Object>(connect_data);
    ELLE_LOG("%s: got connect message: %s",
             *this,
             elle::json::pretty_print(connect_msg));
    this->_login_response(socket);
    reactor::wait(this->client_connected());
    ELLE_LOG("start serving ping");
    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      scope.run_background("pinger", [&]
      {
        while (true)
        {
          elle::json::Object msg;
          msg["notification_type"] = int(PING_NOTIFICATION);
          ELLE_LOG("send ping");
          elle::json::write(socket, msg);
          reactor::sleep(this->_period);
        }
      });
      scope.run_background("ping checker", [&]
      {
        auto previous = boost::posix_time::microsec_clock::local_time();
        while (true)
        {
          auto data_read = elle::json::read(socket);
          auto now = boost::posix_time::microsec_clock::local_time();
          auto ping = boost::any_cast<elle::json::Object>(data_read);
          auto diff = now - previous;
          ELLE_LOG("got ping after %s: %s",
                   diff,
                   elle::json::pretty_print(ping));
          BOOST_CHECK_LT(diff, this->_period * 11 / 10);
          previous = now;
          ++this->_ping_received;;
        }
      });
      scope.wait();
    };
  }

private:
  ELLE_ATTRIBUTE(boost::posix_time::time_duration, period);
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
      reactor::sleep(run_time);
    });
    scope.wait(run_time);
    BOOST_CHECK_EQUAL(client->reconnected(), 0);
    BOOST_CHECK_LE(std::abs(tropho.ping_received() - periods), 1);
  };
}

/*--------.
| No ping |
`--------*/

// Check a client not receiving pings reconnects every 2 periods.

class NoPingTrophonius:
  public Trophonius
{
public:
  NoPingTrophonius(boost::posix_time::time_duration const& period):
    Trophonius(),
    _ping_received(0),
    _period(period)
  {}

  ELLE_ATTRIBUTE_R(int, ping_received);

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    reactor::wait(this->poked());
    auto connect_data = elle::json::read(socket);
    auto connect_msg = boost::any_cast<elle::json::Object>(connect_data);
    ELLE_DEBUG("%s: got connect message: %s",
               *this,
               elle::json::pretty_print(connect_msg));
    this->_login_response(socket);
    auto previous = boost::posix_time::microsec_clock::local_time();
    ELLE_LOG("serving no ping");
    try
    {
      while (true)
      {
        auto data_read = elle::json::read(socket);
        auto now = boost::posix_time::microsec_clock::local_time();
        auto ping = boost::any_cast<elle::json::Object>(data_read);
        auto diff = now - previous;
        ELLE_LOG("got ping after %s: %s", diff, elle::json::pretty_print(ping));
        BOOST_CHECK_LT(diff, this->_period * 11 / 10);
        previous = now;
        ++this->_ping_received;
      }
    }
    catch(reactor::network::ConnectionClosed const&)
    {
      // As the client will not be receiving any pings, it will disconnect
      auto disconnection_time =
        boost::posix_time::microsec_clock::local_time() - previous;
      ELLE_LOG("%s: disconnection after %s", *this, disconnection_time);
      BOOST_CHECK_GE(this->ping_received(), 1);
      BOOST_CHECK_LT(disconnection_time, this->_period * 22 / 10);
    }
  }

private:
  ELLE_ATTRIBUTE(boost::posix_time::time_duration, period);
};

ELLE_TEST_SCHEDULED(no_ping)
{
  boost::posix_time::time_duration const period = 100_ms;
  boost::posix_time::time_duration const run_time = 3_sec;
  int periods = run_time.total_milliseconds() / period.total_milliseconds();

  NoPingTrophonius tropho(period);

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
      while (true)
      {
        reactor::wait(client->poked());
        tropho.poked().signal();
      }
    });
    scope.run_background("client", [&]
    {
      reactor::wait(tropho.poked());
      client->ping_period(period);
      client->connect("0", "0", "0");
    });
    scope.wait(run_time);
    // Approximate test as we don't know how long a poke, connect, disconnect
    // cycle will take.
    BOOST_CHECK(std::abs(client->reconnected() - (periods / 2)) < 3);
    scope.terminate_now();
  };
}

/*-------------.
| Reconnection |
`-------------*/

// Ensure that we read the correct notification on losing connection and
// reconnecting.

class ReconnectionTrophonius:
  public Trophonius
{
public:
  ReconnectionTrophonius():
    Trophonius(),
    _first(true)
  {}

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    reactor::wait(this->poked());
    auto connect_data = elle::json::read(socket);
    auto connect_msg = boost::any_cast<elle::json::Object>(connect_data);
    ELLE_DEBUG("%s: got connect message: %s",
               *this,
               elle::json::pretty_print(connect_msg));
    this->_login_response(socket);
    if (this->_first)
    {
      this->_first = false;
      this->_send_notification(socket, "0");
      this->_send_notification(socket, "1");
      ELLE_LOG("wait for disconnection")
      // Read pings until disconnected.
      {
        try
        {
          while (true)
          {
            auto ping = elle::json::read(socket);
          }
        }
        catch (reactor::network::ConnectionClosed const&)
        {}
      }
    }
    else
    {
      this->_send_notification(socket, "2");
      // Wait forever.
      reactor::Scheduler::scheduler()->current()->Waitable::wait();
    }
  }

private:
  ELLE_ATTRIBUTE(bool, first);
};

ELLE_TEST_SCHEDULED(reconnection)
{
  reactor::Barrier reconnecting;
  reactor::Signal end_test;
  bool synchronized = false;
  bool first_connect = true;
  ReconnectionTrophonius tropho;
  using namespace infinit::oracles::trophonius;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    std::unique_ptr<Client> client;
    client.reset(new Client(
      "127.0.0.1",
      tropho.port(),
      [&] (bool connected) // connect callback
      {
        if (connected)
        {
          if (first_connect)
          {
            // Wait to check that the client doesn't get notification '2' yet.
            reactor::sleep(1_sec);
            synchronized = true;
            first_connect = false;
          }
        }
        else
        {
          ELLE_LOG("client got disconnected");
          reconnecting.open();
        }
      },
      [] (void) {})  // reconnection failed callback
    );
    client->ping_period(1_sec);
    scope.run_background("forward pokes", [&]
    {
      while (true)
      {
        reactor::wait(client->poked());
        tropho.poked().signal();
      }
    });
    scope.run_background("end test", [&]
    {
      reactor::wait(end_test);
      scope.terminate_now();
    });
    scope.run_background("initial poke", [&]
    {
      BOOST_CHECK(client->poke());
      tropho.poked().signal();
    });
    scope.run_background("client", [&]
    {
      reactor::wait(tropho.poked());
      ELLE_LOG("connect");
      client->connect("0", "0", "0");
      tropho.client_connected().signal();
      ELLE_LOG("read notification 0");
      {
        auto notification =
          elle::cast<MessageNotification>::runtime(client->poll());
          BOOST_CHECK_EQUAL(notification->message, "0");
      }
      reactor::wait(reconnecting);
      reactor::wait(client->connected());
      ELLE_LOG("waited for ping timeout")
      {
        // Check notification 1 was discarded
        ELLE_LOG("read notification 2")
        {
          auto notification =
            elle::cast<MessageNotification>::runtime(client->poll());
          BOOST_CHECK(synchronized);
          BOOST_CHECK_EQUAL(notification->message, "2");
          end_test.signal();
        }
      }
    });
    scope.wait();
    scope.terminate_now();
  };
}

/*---------------------------.
| Connection callback throws |
`---------------------------*/

// Check that the reconnection callback is called and that it throws.

class SilentTrophonius:
  public Trophonius
{
protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    reactor::wait(this->poked());
    auto connect_data = elle::json::read(socket);
    auto connect_msg = boost::any_cast<elle::json::Object>(connect_data);
    ELLE_DEBUG("%s: got connect message: %s",
               *this,
               elle::json::pretty_print(connect_msg));
    this->_login_response(socket);
    // Wait forever.
    reactor::Scheduler::scheduler()->current()->Waitable::wait();
  }
};

ELLE_TEST_SCHEDULED_THROWS(connection_callback_throws, std::runtime_error)
{
  bool beacon = false;
  SilentTrophonius tropho;
  using namespace infinit::oracles::trophonius;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    std::unique_ptr<Client> client;
    client.reset(new Client(
      "127.0.0.1",
      tropho.port(),
      [&] (bool connected) // connect callback
      {
        beacon = true;
        throw std::runtime_error("sync failed");
      },
      [] (void) {})  // reconnection failed callback
    );
    client->ping_period(100_ms);
    scope.run_background("initial poke", [&]
    {
      BOOST_CHECK(client->poke());
      tropho.poked().signal();
    });
    scope.run_background("client", [&]
    {
      reactor::wait(client->poked());
      ELLE_LOG("connect");
      client->connect("0", "0", "0");
      reactor::sleep(1_sec);
    });
    scope.wait();
  };
  BOOST_CHECK(beacon);
}

/*-----------------------------.
| Reconnection failed callback |
`-----------------------------*/

// Simulate reconnection on different network where there is no access to
// Trophonius, i.e.: server could be resolved but not contacted. This means that
// the reconnection failed callback should be called.

class ReconnectionFailTrophohius:
  public Trophonius
{
public:
  ReconnectionFailTrophohius():
    Trophonius(),
    _first_connection(true)
  {}

protected:
  virtual
  void
  _accept()
  {
    if (this->_first_connection)
    {
      try
      {
        while (true)
        {
          std::unique_ptr<reactor::network::TCPSocket> socket(
            this->server().accept());
          ELLE_TRACE("%s: accept connection from %s", *this, socket->peer());
          auto poke_read = elle::json::read(*socket);
          auto poke = boost::any_cast<elle::json::Object>(poke_read);
          elle::json::write(*socket, poke);
          ELLE_LOG("%s replied to poke", *this);
          this->_serve(*socket);
        }
      }
      catch (reactor::network::ConnectionClosed const&)
      {
        ELLE_LOG("%s: ignore connection closed on killing accepter", *this);
      }
    }
    else
    {
      // Wait forever to simulate nazi network.
      reactor::Scheduler::scheduler()->current()->Waitable::wait();
    }
  }

  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    reactor::wait(this->poked());
    auto connect_data = elle::json::read(socket);
    auto connect_msg = boost::any_cast<elle::json::Object>(connect_data);
    ELLE_DEBUG("%s: got connect message: %s",
               *this,
               elle::json::pretty_print(connect_msg));
    this->_login_response(socket);
    // Wait forever.
    reactor::Scheduler::scheduler()->current()->Waitable::wait();
  }

private:
  ELLE_ATTRIBUTE(bool, first_connection);
};

ELLE_TEST_SCHEDULED(reconnection_failed_callback)
{
  ReconnectionFailTrophohius tropho;
  reactor::Signal end_test;
  bool callback_called = false;
  using namespace infinit::oracles::trophonius;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    std::unique_ptr<Client> client;
    client.reset(new Client(
      "127.0.0.1",
      tropho.port(),
      [] (bool) {}, // connect callback
      [&] (void)    // reconnection failed callback
      {
        callback_called = true;
        end_test.signal();
      })
    );
    client->ping_period(100_ms);
    scope.run_background("initial poke", [&]
    {
      BOOST_CHECK(client->poke());
      tropho.poked().signal();
    });
    scope.run_background("client", [&]
    {
      reactor::wait(tropho.poked());
      ELLE_LOG("connect");
      client->connect("0", "0", "0");
      // Wait forever.
      reactor::Scheduler::scheduler()->current()->Waitable::wait();
    });
    scope.run_background("end test", [&]
    {
      reactor::wait(end_test);
      scope.terminate_now();
    });
    scope.wait();
  };
  BOOST_CHECK(callback_called);
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 15 : 3;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(poke), 0, timeout);
  suite.add(BOOST_TEST_CASE(notification), 0, timeout);
  suite.add(BOOST_TEST_CASE(ping), 0, 2 * timeout);
  suite.add(BOOST_TEST_CASE(no_ping), 0, 2 * timeout);
  suite.add(BOOST_TEST_CASE(reconnection), 0, 2 * timeout);
  suite.add(BOOST_TEST_CASE(connection_callback_throws), 0, timeout);
  suite.add(BOOST_TEST_CASE(reconnection_failed_callback), 0, 2 * timeout);
}
