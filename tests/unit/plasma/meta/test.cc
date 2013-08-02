#define BOOST_TEST_MODULE meta
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <elle/log.hh>

#include <reactor/network/buffer.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/scheduler.hh>
#include <reactor/semaphore.hh>

#include <plasma/meta/Client.hh>

ELLE_LOG_COMPONENT("infinit.plasma.meta.test");

// static
// void
// sleep(boost::posix_time::time_duration const& d)
// {
//   reactor::Scheduler::scheduler()->current()->sleep(d);
// }

static
void
wait(reactor::Waitable& w)
{
  reactor::Scheduler::scheduler()->current()->wait(w);
}

BOOST_AUTO_TEST_CASE(debug)
{
  ELLE_LOG("test debug query");
  reactor::Scheduler sched;

  reactor::Semaphore sync_client;
  reactor::Semaphore sync_server;

  int port = -1;
  auto serv = [&]
  {
    reactor::network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    sync_client.release(); // Listening
    std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
    ELLE_LOG("connection accepted");

    char buf[512];
    size_t bytes = socket->read_some(
      reactor::network::Buffer(buf, sizeof(buf)), 1_sec);
    std::string data(buf, bytes);
    ELLE_LOG("read: %s", data);

    std::string resp("HTTP/1.0 200 OK\n\n{\"success\": true}\n");
    ELLE_LOG("write: %s", resp);
    socket->write(reactor::network::Buffer(resp));
  };
  reactor::Thread s(sched, "server", serv);

  auto client = [&]
  {
    wait(sync_client); // Listening
    plasma::meta::Client c("127.0.0.1", port);
    c.debug();
  };
  reactor::Thread c(sched, "client", client);
  sched.run();
}

BOOST_AUTO_TEST_CASE(timeout)
{
  ELLE_LOG("test debug query timeout");
  reactor::Scheduler sched;

  reactor::Semaphore sync_client;
  reactor::Semaphore sync_server;

  int port = -1;
  auto serv = [&]
  {
    reactor::network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    sync_client.release(); // Listening
    std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
    reactor::Scheduler::scheduler()->current()->sleep(3600_sec);
  };
  reactor::Thread s(sched, "server", serv);

  auto client = [&]
  {
    wait(sync_client); // Listening
    plasma::meta::Client c("127.0.0.1", port);
    BOOST_CHECK_THROW(c.debug(), std::runtime_error);
    s.terminate();
  };
  reactor::Thread c(sched, "client", client);
  sched.run();
}
