#define BOOST_TEST_MODULE trophonius

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

BOOST_AUTO_TEST_CASE(notification)
{
  static int const reconnections = 5;

  reactor::Scheduler sched;
  int port = -1;
  namespace network = reactor::network;

  reactor::Semaphore sync_client;
  reactor::Semaphore sync_server;

  auto serv = [&]
  {
    network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    sync_client.release(); // Listening
    for (int i = 0; i < reconnections; i++)
    {
      std::unique_ptr<network::TCPSocket> socket{server.accept()};
      ELLE_LOG("connection accepted");

      std::string buf(512, '\0');

      using namespace reactor;
      size_t bytes;

      bytes = socket->read_some(network::Buffer(buf), 1_sec);
      buf.resize(bytes);
      ELLE_LOG("read: %s", buf);

      std::string data =
        "{\"notification_type\": 217,"
        "\"sender_id\": \"id\", "
        "\"message\": \"hello\"}\n";

      ELLE_LOG("write: %s", data);
      socket->write(network::Buffer(data));
      sync_client.release(); // Answered
      wait(sync_server); // Polled
      sync_client.release(); // Disconnected
    }
  };
  reactor::Thread s{sched, "server", std::move(serv)};

  auto client = [&]
  {
    using namespace plasma::trophonius;
    plasma::trophonius::Client c("127.0.0.1", port, [] (bool) {});
    wait(sync_client); // Listening
    ELLE_LOG("connect");
    c.connect("", "", "");
    for (int i = 0; i < reconnections; ++i)
    {
      wait(sync_client); // Answered
      BOOST_CHECK_EQUAL(c.reconnected(), i);
      ELLE_LOG("poll notifications");
      std::unique_ptr<Notification> notif = c.poll();
      BOOST_CHECK(notif);
      ELLE_LOG("got notification: %s", notif->notification_type);
      BOOST_CHECK_EQUAL(notif->notification_type,
                        plasma::trophonius::NotificationType::message);
      sync_server.release(); // Polled
      wait(sync_client); // Disconnected
    }
  };
  reactor::Thread c{sched, "client", std::move(client)};
  sched.run();
}

BOOST_AUTO_TEST_CASE(ping)
{
  boost::posix_time::time_duration const period = 100_ms;
  boost::posix_time::time_duration const run_time = 3_sec;
  int periods = run_time.total_milliseconds() / period.total_milliseconds();

  reactor::Scheduler sched;

  reactor::Semaphore sync_client;
  reactor::Semaphore sync_server;

  int port = -1;
  namespace network = reactor::network;

  auto serv = [&]
  {
    using namespace reactor;
    network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    sync_client.release(); // Listening
    std::unique_ptr<network::TCPSocket> socket{server.accept()};
    ELLE_LOG("connection accepted");

    std::unique_ptr<Thread> ping(sched.every([&] {
          std::string msg = "{\"notification_type\": 208}\n";
          ELLE_LOG("send ping");
          socket->write(network::Buffer(msg));
        }, "ping", period));
    elle::SafeFinally end_ping([&] { ping->terminate_now(); });

    auto previous = boost::posix_time::microsec_clock::local_time();
    int received = 0;
    elle::SafeFinally check([&] {
        BOOST_CHECK_LE(std::abs(received - periods), 1);
      });
    while (true)
    {
      std::string buf(512, '\0');
      size_t bytes = socket->read_some(network::Buffer(buf));
      buf.resize(bytes);
      if (buf[buf.length() - 1] != '\n')
        continue;
      auto now = boost::posix_time::microsec_clock::local_time();
      auto diff = now - previous;
      ELLE_LOG("got ping after %s", diff);
      BOOST_CHECK_LT(diff, period * 11 / 10);
      previous = now;
      ++received;
    }
  };

  auto client_thread = [&]
  {
    plasma::trophonius::Client client("127.0.0.1", port, [] (bool) {});
    elle::SafeFinally check([&] {BOOST_CHECK_EQUAL(client.reconnected(), 0);});
    client.ping_period(period);
    wait(sync_client); // Listening
    client.connect("", "", "");
    while (true)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<plasma::trophonius::Notification> notif = client.poll();
    }
  };

  reactor::Thread s(sched, "server", serv);
  reactor::Thread c(sched, "client", client_thread);
  reactor::Thread j(sched, "janitor", [&] {
      sleep(run_time);
      sched.terminate();
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(noping)
{
  boost::posix_time::time_duration const period = 100_ms;
  boost::posix_time::time_duration const run_time = 3_sec;
  int periods = run_time.total_milliseconds() / period.total_milliseconds();

  reactor::Scheduler sched;

  reactor::Semaphore sync_client;
  reactor::Semaphore sync_server;

  int port = -1;
  namespace network = reactor::network;

  auto serv = [&]
  {
    using namespace reactor;
    network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    sync_client.release(); // Listening
    while (true)
    {
      std::unique_ptr<network::TCPSocket> socket{server.accept()};
      ELLE_LOG("connection accepted");

      auto start = boost::posix_time::microsec_clock::local_time();
      while (true)
      {
        std::string buf(512, '\0');
        size_t bytes = socket->read_some(network::Buffer(buf));
        buf.resize(bytes);
        if (buf[buf.length() - 1] != '\n')
          continue;
        ELLE_LOG("got auth: %s", buf);
        break;
      }
      std::string buf(512, '\0');
      int pings = 0;
      try
      {
        while (true)
        {
          size_t bytes = socket->read_some(network::Buffer(buf));
          buf.resize(bytes);
          if (buf[buf.length() - 1] != '\n')
            continue;
          auto now = boost::posix_time::microsec_clock::local_time();
          auto ping_time = now - start;
          ELLE_LOG("got ping after %s: %s", ping_time, buf);
          ++pings;
          BOOST_CHECK_LT(ping_time, period * 11 / 10);
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
        BOOST_CHECK_LT(disconnection_time, period * 22 / 10);
      }
    }
  };

  auto client_thread = [&]
  {
    plasma::trophonius::Client client("127.0.0.1", port, [] (bool) {});
    elle::SafeFinally check([&] {
        BOOST_CHECK_LE(std::abs(client.reconnected() - (periods / 2)), 1);
      });
    client.ping_period(period);
    wait(sync_client); // Listening
    client.connect("", "", "");
    while (true)
    {
      ELLE_LOG("poll notifications")
        std::unique_ptr<plasma::trophonius::Notification> notif = client.poll();
    }
  };

  reactor::Thread s(sched, "server", serv);
  reactor::Thread c(sched, "client", client_thread);
  reactor::Thread j(sched, "janitor", [&] {
      sleep(run_time);
      sched.terminate();
    });
  sched.run();
}

static
void
send_notification(reactor::network::TCPSocket& socket,
                  std::string const& message)
{

  std::string data =
    elle::sprintf("{\"notification_type\": 217,"
                  "\"sender_id\": \"id\", "
                  "\"message\": \"%s\"}\n", message);
  ELLE_LOG("write: %s", data);
  socket.write(reactor::network::Buffer(data));
}

BOOST_AUTO_TEST_CASE(reconnection)
{
  reactor::Scheduler sched;

  reactor::Barrier listening;
  reactor::Barrier reconnecting;

  int port = 0;
  bool synchronized = false;

  reactor::Thread server(
    sched, "server",
    [&]
    {
      reactor::network::TCPServer server(sched);
      server.listen(0);
      port = server.port();
      ELLE_LOG("listen on port %s", port);
      listening.open();
      std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
      send_notification(*socket, "0");
      send_notification(*socket, "1");
      reconnecting.wait();
      socket.reset(server.accept());
      send_notification(*socket, "2");
      reactor::sleep(10_sec);
    });

  reactor::Thread client(
    sched, "client",
    [&]
    {
      listening.wait();
      plasma::trophonius::Client c(
        "127.0.0.1", port,
        [&] (bool connected)
        {
          if (connected)
          {
            reconnecting.open();
            reactor::sleep(500_ms);
            synchronized = true;
          }
        });
      c.ping_period(1_sec);
      ELLE_LOG("connect");
      c.connect("", "", "");
      using plasma::trophonius::MessageNotification;
      auto notif0 = elle::cast<MessageNotification>::runtime(c.poll());
      BOOST_CHECK_EQUAL(notif0->message, "0");
      reconnecting.wait();
      // Check notification 1 was discarded and notification 2 is held until
      // reconnection callback is complete.
      auto notif2 = elle::cast<MessageNotification>::runtime(c.poll());
      BOOST_CHECK(synchronized);
      BOOST_CHECK_EQUAL(notif2->message, "2");
      server.terminate();
    });

  sched.run();
}


BOOST_AUTO_TEST_CASE(connection_callback_throws)
{
  reactor::Scheduler sched;

  reactor::Barrier listening;

  bool beacon = false;
  int port = 0;

  reactor::Thread server(
    sched, "server",
    [&]
    {
      reactor::network::TCPServer server(sched);
      server.listen(0);
      port = server.port();
      ELLE_LOG("listen on port %s", port);
      listening.open();
      reactor::sleep(5_sec);
    });

  reactor::Thread client(
    sched, "client",
    [&]
    {
      listening.wait();
      plasma::trophonius::Client c(
        "127.0.0.1", port,
        [&] (bool connected)
        {
          beacon = true;
          throw std::runtime_error("sync failed");
        });
      c.ping_period(100_ms);
      ELLE_LOG("connect");
      c.connect("", "", "");
      reactor::sleep(5_sec);
      server.terminate();
    });

  BOOST_CHECK_THROW(sched.run(), std::runtime_error);
  BOOST_CHECK(beacon);
}
