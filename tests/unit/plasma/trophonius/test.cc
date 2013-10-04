#define BOOST_TEST_DYN_LINK

#include <boost/date_time/posix_time/date_duration_operators.hpp>
#include <boost/test/unit_test.hpp>

#include <elle/cast.hh>
#include <elle/finally.hh>

#include <reactor/Barrier.hh>
#include <reactor/duration.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/scheduler.hh>
#include <reactor/semaphore.hh>
#include <reactor/signal.hh>
#include <reactor/sleep.hh>
#include <reactor/thread.hh>

#include <memory>
#include <common/common.hh>

#include <fstream>
#include <unistd.h>

#include <plasma/trophonius/Client.hh>

ELLE_LOG_COMPONENT("infinit.plasma.trophonius.test");

static
void
sleep(boost::posix_time::time_duration const& d)
{
  reactor::Scheduler::scheduler()->current()->sleep(d);
}

static
void
wait(reactor::Waitable& w)
{
  reactor::Scheduler::scheduler()->current()->wait(w);
}

class Trophonius
{
public:
  Trophonius():
    _server(),
    _thread(*reactor::Scheduler::scheduler(), "server",
           std::bind(&Trophonius::_accept, this))
  {
    this->_server.listen(0);
    ELLE_LOG("listen on port %s", this->port());
  }

  ~Trophonius()
  {
    this->_thread.terminate_now();
  }

  int
  port() const
  {
    return this->_server.port();
  }

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket) = 0;

  void
  _send_notification(reactor::network::TCPSocket& socket,
                    std::string const& message)
  {
    std::string data =
      elle::sprintf("{\"notification_type\": 217,"
                    "\"sender_id\": \"id\", "
                    "\"message\": \"%s\"}\n", message);
    ELLE_LOG("write: %s", data);
    socket.write(reactor::network::Buffer(data));
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
    std::string data =
    elle::sprintf("{\"notification_type\": -666,"
                  "\"response_code\": %s,"
                  "\"response_details\": \"nothing\"}\n",
                  (success ? "200" : "500"));
    ELLE_LOG("login response: %s", data);
    socket.write(reactor::network::Buffer(data));
  }

private:
  void
  _accept()
  {
    while (true)
    {
      std::unique_ptr<reactor::network::TCPSocket> socket(
        this->_server.accept());
      ELLE_LOG("connection accepted");
      std::string buf(512, '\0');
      using namespace reactor;
      size_t bytes;
      bytes = socket->read_some(network::Buffer(buf), 1_sec);
      buf.resize(bytes);
      ELLE_LOG("read: %s", buf);
      this->_login_response(*socket);
      this->_serve(*socket);
    }
  }

  reactor::network::TCPServer _server;
  reactor::Thread _thread;
};

/*-------------.
| Notification |
`-------------*/

// Check a client can receive simple message notification, and will reconnect
// when the socket is closed.

class NotificationTrophonius:
  public Trophonius
{
protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    this->_send_notification(socket, "hello");
  }
};

static
void
notification()
{
  static int const reconnections = 5;
  auto client = [&]
  {
    NotificationTrophonius tropho;
    using namespace plasma::trophonius;
    plasma::trophonius::Client c("127.0.0.1", tropho.port(), [] (bool) {});
    ELLE_LOG("successful connection");
    c.connect("0", "0", "0");
    for (int i = 0; i < reconnections; ++i)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<Notification> notif = c.poll();
      BOOST_CHECK_EQUAL(c.reconnected(), i);
      BOOST_CHECK(notif);
      ELLE_LOG("got notification: %s", notif->notification_type);
      BOOST_CHECK_EQUAL(notif->notification_type,
                        plasma::trophonius::NotificationType::message);
    }
  };
  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  sched.run();
}

/*-----.
| Ping |
`-----*/

// Check the clients pings every period.

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
    while (true)
    {
      auto& sched = *reactor::Scheduler::scheduler();
      // Ping the client to keep it happy.
      std::unique_ptr<reactor::Thread> ping(sched.every([&] {
            std::string msg = "{\"notification_type\": 208}\n";
            ELLE_LOG("send ping");
            socket.write(reactor::network::Buffer(msg));
          }, "ping", this->_period));
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

static
void
ping()
{
  boost::posix_time::time_duration const period = 100_ms;
  boost::posix_time::time_duration const run_time = 3_sec;
  int periods = run_time.total_milliseconds() / period.total_milliseconds();

  auto client = [&]
  {
    PingTrophonius tropho(period);

    plasma::trophonius::Client client("127.0.0.1", tropho.port(), [] (bool) {});
    client.ping_period(period);
    client.connect("0", "0", "0");
    elle::SafeFinally check([&] {
        BOOST_CHECK_EQUAL(client.reconnected(), 0);
        BOOST_CHECK_LE(std::abs(tropho.ping_received() - periods), 1);
      });
    while (true)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<plasma::trophonius::Notification> notif = client.poll();
    }

  };

  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  reactor::Thread j(sched, "janitor", [&] {
      sleep(run_time);
      sched.terminate();
    });
  sched.run();
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
    _period(period),
    _ping_received(0)
  {}

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    auto start = boost::posix_time::microsec_clock::local_time();
    std::string buf(512, '\0');
    int pings = 0;
    try
    {
      while (true)
      {
        size_t bytes = socket.read_some(reactor::network::Buffer(buf));
        buf.resize(bytes);
        if (buf[buf.length() - 1] != '\n')
          continue;
        auto now = boost::posix_time::microsec_clock::local_time();
        auto ping_time = now - start;
        ELLE_LOG("got ping after %s: %s", ping_time, buf);
        ++pings;
        BOOST_CHECK_LT(ping_time, this->_period * 11 / 10);
        start = now;
      }
    }
    catch (reactor::network::ConnectionClosed const&)
    {
      // The client, not receiving pings, shall disconnect.
      auto disconnection_time =
        boost::posix_time::microsec_clock::local_time() - start;
      ELLE_LOG("disconnection after %s", disconnection_time);
      BOOST_CHECK_GE(pings, 1);
      BOOST_CHECK_LT(disconnection_time, this->_period * 22 / 10);
    }

  }

private:
  boost::posix_time::time_duration _period;
  ELLE_ATTRIBUTE_R(int, ping_received);
};

static
void
noping()
{
  boost::posix_time::time_duration const period = 100_ms;
  boost::posix_time::time_duration const run_time = 3_sec;
  int periods = run_time.total_milliseconds() / period.total_milliseconds();

  auto client = [&]
  {
    NoPingTrophonius tropho(period);
    plasma::trophonius::Client client("127.0.0.1", tropho.port(), [] (bool) {});
    elle::SafeFinally check([&] {
        BOOST_CHECK_LE(std::abs(client.reconnected() - (periods / 2)), 1);
      });
    client.ping_period(period);
    client.connect("0", "0", "0");
    while (true)
    {
      ELLE_LOG("poll notifications")
        std::unique_ptr<plasma::trophonius::Notification> notif = client.poll();
    }
  };

  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  reactor::Thread j(sched, "janitor", [&] {
      sleep(run_time);
      sched.terminate();
    });
  sched.run();
}

/*-------------.
| Reconnection |
`-------------*/

class ReconnectionTrophonius:
  public Trophonius
{
public:
  ReconnectionTrophonius():
    _first(true)
  {}

protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    if (this->_first)
    {
      this->_first = false;
      this->_send_notification(socket, "0");
      this->_send_notification(socket, "1");
      ELLE_LOG("Wait for disconnection")
        // Read pings until disconnected.
        try
        {
          while (true)
          {
            std::string buf(512, '\0');
            socket.read_some(reactor::network::Buffer(buf));
          }
        }
        catch (reactor::network::ConnectionClosed const&)
        {}
    }
    else
    {
      this->_send_notification(socket, "2");
      // Wait forever.
      reactor::Scheduler::scheduler()->current()->Waitable::wait();
    }
  }

private:
  bool _first;
};

static
void
reconnection()
{
  using plasma::trophonius::MessageNotification;
  reactor::Barrier reconnecting;
  bool synchronized = false;
  auto client = [&]
    {
      ReconnectionTrophonius tropho;
      plasma::trophonius::Client c(
        "127.0.0.1", tropho.port(),
        [&] (bool connected)
        {
          if (connected)
          {
            // Wait to check the client doesn't get the '2' notification yet.
            reactor::sleep(1_sec);
            synchronized = true;
          }
          else
          {
            reconnecting.open();
          }
        });
      c.ping_period(1_sec);
      ELLE_LOG("connect")
        c.connect("0", "0", "0");
      ELLE_LOG("read notification 0")
      {
        auto notif = elle::cast<MessageNotification>::runtime(c.poll());
        BOOST_CHECK_EQUAL(notif->message, "0");
      }
      ELLE_LOG("wait for ping timeout")
        reconnecting.wait();
      // Check notification 1 was discarded and notification 2 is held until
      // reconnection callback is complete.
      ELLE_LOG("read notification 2");
      {
        auto notif = elle::cast<MessageNotification>::runtime(c.poll());
        BOOST_CHECK(synchronized);
        BOOST_CHECK_EQUAL(notif->message, "2");
      }
    };
  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  sched.run();
}

/*---------------------------.
| Connection callback throws |
`---------------------------*/

class SilentTrophonius:
  public Trophonius
{
protected:
  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    // Wait forever.
    reactor::Scheduler::scheduler()->current()->Waitable::wait();
  }
};

static
void
connection_callback_throws()
{
  bool beacon = false;
  auto client =
    [&]
    {
      SilentTrophonius tropho;
      plasma::trophonius::Client c(
        "127.0.0.1", tropho.port(),
        [&] (bool connected)
        {
          beacon = true;
          throw std::runtime_error("sync failed");
        });
      c.ping_period(100_ms);
      ELLE_LOG("connect")
        c.connect("0", "0", "0");
      reactor::sleep(1_sec);
    };
  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  BOOST_CHECK_THROW(sched.run(), std::runtime_error);
  BOOST_CHECK(beacon);
}

/*--------------------.
| Reconnection denied |
`--------------------*/

class RejectTrophonius:
  public Trophonius
{
public:
  RejectTrophonius(bool first = true):
    _first(first)
  {}

protected:
  using Trophonius::_login_response;
  virtual
  void
  _login_response(reactor::network::TCPSocket& socket)
  {
    if (this->_first)
    {
      this->_first = false;
      this->_login_response(socket, true);
    }
    else
      this->_login_response(socket, false);
  }

  virtual
  void
  _serve(reactor::network::TCPSocket& socket)
  {
    // Read pings until disconnected.
    try
    {
      while (true)
      {
        std::string buf(512, '\0');
        socket.read_some(reactor::network::Buffer(buf));
      }
    }
    catch (reactor::network::ConnectionClosed const&)
    {}
  }

private:
  bool _first;
};

static
void
reconnection_denied()
{
  int connected_count = 0;
  int disconnected_count = 0;
  auto client =
    [&]
    {
      RejectTrophonius tropho;
      plasma::trophonius::Client c(
        "127.0.0.1", tropho.port(),
        [&] (bool connected)
        {
          if (connected)
            ++connected_count;
          else
            ++disconnected_count;
        });
      c.ping_period(50_ms);
      ELLE_LOG("connect")
        c.connect("0", "0", "0");
      // Server should have reject the connection because login information are
      // now incorrect.  Next poll will throw.
      BOOST_CHECK_THROW(c.poll(), elle::Exception);
    };
  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  sched.run();
  BOOST_CHECK_EQUAL(connected_count, 1);
  BOOST_CHECK_EQUAL(disconnected_count, 1);
}

/*--------------------.
| Connection rejected |
`--------------------*/

// Check connect throws if the authentication is rejected.

static
void
connection_rejected()
{
  auto client =
    [&]
    {
      RejectTrophonius tropho(false);
      plasma::trophonius::Client c("127.0.0.1", tropho.port(), [&] (bool) {});
      BOOST_CHECK_THROW(c.connect("0", "0", "0"), elle::Exception);
      BOOST_CHECK_THROW(c.poll(), elle::Exception);
    };
  reactor::Scheduler sched;
  reactor::Thread c(sched, "client", client);
  sched.run();
}

static
bool
test_suite()
{
  auto& ts = boost::unit_test::framework::master_test_suite();
  ts.add(BOOST_TEST_CASE(notification), 0, 3);
  ts.add(BOOST_TEST_CASE(ping), 0, 10);
  ts.add(BOOST_TEST_CASE(noping), 0, 10);
  ts.add(BOOST_TEST_CASE(reconnection), 0, 10);
  ts.add(BOOST_TEST_CASE(connection_callback_throws), 0, 3);
  ts.add(BOOST_TEST_CASE(reconnection_denied), 0, 3);
  ts.add(BOOST_TEST_CASE(connection_rejected), 0, 3);
  return true;
}

int
main(int argc, char** argv)
{
  return ::boost::unit_test::unit_test_main(test_suite, argc, argv);
}
