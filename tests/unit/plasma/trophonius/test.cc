#define BOOST_TEST_MODULE trophonius
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <elle/system/Process.hh>
#include <reactor/thread.hh>
#include <reactor/scheduler.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/network/tcp-socket.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/sleep.hh>

#include <memory>
#include <common/common.hh>

#include <fstream>
#include <unistd.h>

#include <plasma/trophonius/Client.hh>

void
sleep(int sec)
{
  using namespace reactor;
  reactor::Sleep op(*reactor::Scheduler::scheduler(),
                    boost::posix_time::seconds(sec));

  op.run();
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
    std::cout << "listen on port " << port << std::endl;
    for (int i = 0; i < 2; i++)
    {
      std::unique_ptr<network::TCPSocket> socket{server.accept()};
      std::cout << "Connection accepted" << std::endl;

      std::string buf(512, '\0');

      using namespace reactor;
      size_t bytes;

      bytes = socket->read_some(network::Buffer(buf), 1_sec);
      buf.resize(bytes);
      std::cout << "Read: " << buf << std::endl;

      std::string data =
        R"({"notification_type": 217, "sender_id": "id", "message": "hello"})";

      socket->write(network::Buffer(data));

      sleep(5);
    }
  };
  reactor::Thread s{sched, "server", std::move(serv)};

  auto client = [&]
  {
    using namespace plasma::trophonius;
    sleep(1);
    plasma::trophonius::Client c("127.0.0.1", port, [] {});

    sleep(1);
    c.connect("", "", "");
    int msg = 0;
    while (1)
    {
      std::cout << "polling notifications" << std::endl;
      std::unique_ptr<Notification> notif = c.poll();
      sleep(1);

      if (!notif)
        continue;
      msg++;
      std::cout << msg << " HAVE NOTIF: " << notif->notification_type << std::endl;
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
    std::cout << "listen on port " << port << std::endl;
    std::unique_ptr<network::TCPSocket> socket{server.accept()};
    std::cout << "Connection accepted" << std::endl;

    std::string buf(512, '\0');
    size_t bytes = socket->read_some(network::Buffer(buf));
    buf.resize(bytes);
    std::cout << "Server read: " << buf << std::endl;

    auto send_ping = [&]
    {
      sleep(30);
      std::string msg = "{\"notification_type\": 208}\n";
      socket->write(network::Buffer(msg));
      sleep(30);
      msg = "{\"notification_type\": 208}\n";
      socket->write(network::Buffer(msg));
    };
    reactor::Thread ping{sched, "ping", std::move(send_ping)};

    for (int i = 0; i < 2; i++)
    {
      std::string buf(512, '\0');

      bytes = socket->read_some(network::Buffer(buf));
      buf.resize(bytes);
      std::cout << "Server read: " << buf << std::endl;
    }
    auto* this_thread = sched.current();
    this_thread->wait(ping);
    client_thread->terminate_now();
  };
  reactor::Thread s{sched, "server", std::move(serv)};

  auto client = [&]
  {
    using namespace plasma::trophonius;
    sleep(1);
    plasma::trophonius::Client c("127.0.0.1", port, [] {});

    sleep(1);
    c.connect("", "", "");
    while (1)
    {
      std::cout << "polling notifications" << std::endl;
      std::unique_ptr<Notification> notif = c.poll();
      sleep(1);

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
    std::cout << "listen on port " << port << std::endl;
    for (int times = 0; times < 2; times++)
    {
      std::unique_ptr<network::TCPSocket> socket{server.accept()};
      std::cout << "Connection accepted" << std::endl;

      try
      {
        std::string buf(512, '\0');
        size_t bytes = socket->read_some(network::Buffer(buf));
        buf.resize(bytes);
        std::cout << "Server read: " << buf << std::endl;

        for (int i = 0; i < 2; i++)
        {
          std::string buf(512, '\0');

          bytes = socket->read_some(network::Buffer(buf));
          buf.resize(bytes);
          std::cout << "Server read: " << buf << std::endl;
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
    sleep(1);
    plasma::trophonius::Client c("127.0.0.1", port, [] {});

    sleep(1);
    c.connect("", "", "");
    while (1)
    {
      std::cout << "polling notifications" << std::endl;
      std::unique_ptr<Notification> notif = c.poll();
      sleep(1);

      if (!notif)
        continue;
    }
  };
  reactor::Thread c{sched, "client", std::move(client)};
  client_thread = &c;
  sched.run();
}
