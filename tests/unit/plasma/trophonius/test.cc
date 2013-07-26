#define BOOST_TEST_MODULE trophonius
#define BOOST_TEST_DYN_LINK
#include <boost/date_time/posix_time/date_duration_operators.hpp>
#include <boost/test/unit_test.hpp>

#include <elle/finally.hh>
#include <elle/system/Process.hh>

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

BOOST_AUTO_TEST_CASE(test)
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
    plasma::trophonius::Client c("127.0.0.1", port, [] {});
    wait(sync_client); // Listening
    c.connect("", "", "");
    for (int i = 0; i < reconnections; ++i)
    {
      wait(sync_client); // Answered
      ELLE_LOG("poll notifications");
      std::unique_ptr<Notification> notif = c.poll();
      BOOST_CHECK(notif);
      ELLE_LOG("got notification: %s", notif->notification_type);
      BOOST_CHECK_EQUAL(notif->notification_type,
                        plasma::trophonius::NotificationType::message);
      sync_server.release(); // Polled
      wait(sync_client); // Disconnected
      if (i == reconnections - 1)
      {
        BOOST_CHECK_THROW(c.poll(), std::runtime_error);
      }
      else
        BOOST_CHECK(!c.poll());
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
    elle::Finally end_ping([&] { ping->terminate_now(); });

    auto previous = boost::posix_time::microsec_clock::local_time();
    int received = 0;
    elle::Finally check([&] {
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
    plasma::trophonius::Client client("127.0.0.1", port, [] {});
    elle::Finally check([&] { BOOST_CHECK_EQUAL(client.reconnected(), 0); });
    client.ping_period(period);
    wait(sync_client); // Listening
    client.connect("", "", "");
    while (true)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<plasma::trophonius::Notification> notif = client.poll();
      sleep(period);
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
        ELLE_LOG("got auth");
        break;
      }
      while (true)
      {
        std::string buf(512, '\0');
        size_t bytes = socket->read_some(network::Buffer(buf));
        buf.resize(bytes);
        if (buf[buf.length() - 1] != '\n')
          continue;
        auto ping_time = boost::posix_time::microsec_clock::local_time() - start;
        ELLE_LOG("got ping after %s", ping_time);
        BOOST_CHECK_LT(ping_time, period * 11 / 10);
        // The client, not receiving pings, shall disconnect.
        BOOST_CHECK_THROW(socket->read_some(network::Buffer(buf)),
                          std::runtime_error);
        auto disconnection_time =
          boost::posix_time::microsec_clock::local_time() - start;
        ELLE_LOG("disconnection after %s", disconnection_time);
        BOOST_CHECK_LT(disconnection_time, period * 22 / 10);
        break;
      }
    }
  };

  auto client_thread = [&]
  {
    plasma::trophonius::Client client("127.0.0.1", port, [] {});
    elle::Finally check([&] {
        BOOST_CHECK_LE(client.reconnected() - (periods / 2), 1);
      });
    client.ping_period(period);
    wait(sync_client); // Listening
    client.connect("", "", "");
    while (true)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<plasma::trophonius::Notification> notif = client.poll();
      sleep(period);
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
