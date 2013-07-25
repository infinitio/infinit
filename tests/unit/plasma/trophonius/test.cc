#define BOOST_TEST_MODULE trophonius
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
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
  reactor::Scheduler sched;

  reactor::Semaphore sync_client;
  reactor::Semaphore sync_server;

  int port = -1;
  namespace network = reactor::network;
  reactor::Thread* client_thread = nullptr;

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

    auto send_ping = [&]
    {
      sleep(1_sec);
      std::string msg = "{\"notification_type\": 208}\n";
      socket->write(network::Buffer(msg));
      sleep(1_sec);
      msg = "{\"notification_type\": 208}\n";
      socket->write(network::Buffer(msg));
    };
    reactor::Thread ping{sched, "ping", std::move(send_ping)};

    auto* this_thread = sched.current();
    this_thread->wait(ping);
    client_thread->terminate_now();
  };
  reactor::Thread s{sched, "server", std::move(serv)};

  auto client = [&]
  {
    using namespace plasma::trophonius;
    plasma::trophonius::Client c("127.0.0.1", port, [] {});
    c.ping_period(1_sec);
    wait(sync_client); // Listening
    c.connect("", "", "");
    while (1)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<Notification> notif = c.poll();
      sleep(1_sec);

      if (!notif)
        continue;
    }
  };
  reactor::Thread c{sched, "client", std::move(client)};
  client_thread = &c;
  sched.run();
}

BOOST_AUTO_TEST_CASE(noping)
{
  reactor::Scheduler sched;
  int port = -1;
  namespace network = reactor::network;
  reactor::Thread* client_thread = nullptr;

  auto serv = [&]
  {
    using namespace reactor;
    network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    for (int times = 0; times < 2; times++)
    {
      std::unique_ptr<network::TCPSocket> socket{server.accept()};
      ELLE_LOG("connection accepted");

      try
      {
        std::string buf(512, '\0');
        size_t bytes = socket->read_some(network::Buffer(buf));
        buf.resize(bytes);
        ELLE_LOG("read: %s", buf);

        for (int i = 0; i < 2; i++)
        {
          std::string buf(512, '\0');

          bytes = socket->read_some(network::Buffer(buf));
          buf.resize(bytes);
          ELLE_LOG("read: %s", buf);
        }
      }
      catch (reactor::network::ConnectionClosed const&)
      {
        //continue;
      }
    }
    client_thread->terminate_now();
  };
  reactor::Thread s{sched, "server", std::move(serv)};

  auto client = [&]
  {
    using namespace plasma::trophonius;
    sleep(1_sec);
    plasma::trophonius::Client c("127.0.0.1", port, [] {});

    sleep(1_sec);
    c.connect("", "", "");
    while (1)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<Notification> notif = c.poll();
      sleep(1_sec);

      if (!notif)
        continue;
    }
  };
  reactor::Thread c{sched, "client", std::move(client)};
  client_thread = &c;
  sched.run();
}
