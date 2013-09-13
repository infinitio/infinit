#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <elle/log.hh>

#include <reactor/Barrier.hh>
#include <reactor/Scope.hh>
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

static
void
_debug_queries(int count)
{
  ELLE_LOG("test multiple debug queries");
  reactor::Scheduler sched;
  reactor::Barrier listening;

  int port = -1;
  auto serv = [&]
  {
    reactor::network::TCPServer server{sched};

    server.listen(0);
    port = server.port();
    ELLE_LOG("listen on port %s", port);
    listening.open();

    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      int answered = 0;
      reactor::Barrier done;
      for (int i = 0; i < count; ++i)
      {
        auto handler = [s = server.accept(), count, &answered, &done]
          {
            std::unique_ptr<reactor::network::TCPSocket> socket(s);
            ELLE_LOG("connection accepted");

            char buf[512];
            size_t bytes = socket->read_some(
              reactor::network::Buffer(buf, sizeof(buf)), 1_sec);
            std::string data(buf, bytes);
            ELLE_LOG("read: %s", data);

            std::string resp("HTTP/1.0 200 OK\n\n{\"success\": true}\n");
            ELLE_LOG("write: %s", resp);
            socket->write(reactor::network::Buffer(resp));
            if (++answered == count)
              done.open();
          };
        scope.run_background(elle::sprintf("handler %s", i), handler);
      }
      wait(done);
    };
  };
  reactor::Thread s(sched, "server", serv);

  auto client = [&]
  {
    wait(listening); // Listening
    plasma::meta::Client c("127.0.0.1", port);
    ELLE_LOG("send debug request")
      c.debug();
    ELLE_LOG("debug request completed");
  };
  for (int i = 0; i < count; ++i)
    new reactor::Thread(sched, elle::sprintf("client %s", i), client, true);
  sched.run();
}

static
void
debug_query()
{
  _debug_queries(1);
}

static
void
debug_queries()
{
  _debug_queries(30);
}

static
void
timeout()
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


static
bool
test_suite()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(debug_query));
  suite.add(BOOST_TEST_CASE(debug_queries));
  suite.add(BOOST_TEST_CASE(timeout));
  return true;
}

int
main(int argc, char** argv)
{
  return ::boost::unit_test::unit_test_main(test_suite, argc, argv);
}
