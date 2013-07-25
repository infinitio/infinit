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

BOOST_AUTO_TEST_CASE(test)
{
  reactor::Scheduler sched;
  int port = -1;
  namespace network = reactor::network;

  auto serv = [&]
  {
    network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    for (int i = 0; i < 2; i++)
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
        R"({"notification_type": 217, "sender_id": "id", "message": "hello"})";

      ELLE_LOG("write: %s", data);
      socket->write(network::Buffer(data));
      sleep(5_sec);
    }
  };
  reactor::Thread s{sched, "server", std::move(serv)};

  auto client = [&]
  {
    using namespace plasma::trophonius;
    sleep(1_sec);
    plasma::trophonius::Client c("127.0.0.1", port, [] {});

    sleep(1_sec);
    c.connect("", "", "");
    int msg = 0;
    while (1)
    {
      ELLE_LOG("poll notifications");
      std::unique_ptr<Notification> notif = c.poll();
      sleep(1_sec);

      if (!notif)
        continue;
      msg++;
      ELLE_LOG("got notification: %s", notif->notification_type);
      if (msg == 2)
        break;
    }
  };
  reactor::Thread c{sched, "client", std::move(client)};
  sched.run();
}

BOOST_AUTO_TEST_CASE(ping)
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
    std::unique_ptr<network::TCPSocket> socket{server.accept()};
    ELLE_LOG("connection accepted");

    std::string buf(512, '\0');
    size_t bytes = socket->read_some(network::Buffer(buf));
    buf.resize(bytes);
    ELLE_LOG("read: %s",  buf);

    auto send_ping = [&]
    {
      sleep(30_sec);
      std::string msg = "{\"notification_type\": 208}\n";
      socket->write(network::Buffer(msg));
      sleep(30_sec);
      msg = "{\"notification_type\": 208}\n";
      socket->write(network::Buffer(msg));
    };
    reactor::Thread ping{sched, "ping", std::move(send_ping)};

    for (int i = 0; i < 2; i++)
    {
      std::string buf(512, '\0');

      bytes = socket->read_some(network::Buffer(buf));
      buf.resize(bytes);
      ELLE_LOG("read: %s", buf);
    }
    auto* this_thread = sched.current();
    this_thread->wait(ping);
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
