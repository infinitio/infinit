#define BOOST_TEST_MODULE heartbeat
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <elle/log.hh>
#include <elle/system/Process.hh>

#include <reactor/thread.hh>
#include <reactor/scheduler.hh>
#include <reactor/network/udp-socket.hh>
#include <reactor/network/udt-socket.hh>
#include <reactor/network/buffer.hh>
#include <reactor/sleep.hh>

#include <string>
#include <sstream>
#include <algorithm>
#include <memory>

ELLE_LOG_COMPONENT("testing.heartbeat");

namespace network = reactor::network;

namespace heartbeat {

static
void
_sleep(int seconds)
{
  auto& sched = *reactor::Scheduler::scheduler();
  reactor::Sleep s{sched, boost::posix_time::seconds{seconds}};
  s.run();
}

static
void
run()
{
  auto& sched = *reactor::Scheduler::scheduler();

  ELLE_LOG("connecting to localhost");
  network::UDTSocket socket(sched, "127.0.0.1", 9090);

  for (int i = 0; i < 10; ++i)
  {
    std::string msg{"echo"};
    socket.write(network::Buffer{msg});
    msg.resize(512);
    size_t bytes = socket.read_some(network::Buffer{msg});
    msg.resize(bytes);
    ELLE_DUMP("received %s", msg);
    _sleep(1);
  }
}

} /* heartbeat */

BOOST_AUTO_TEST_CASE(heartbeat_test)
{
  reactor::Scheduler sched;

  auto fn = [&]
  {
    auto pc = elle::system::process_config(elle::system::normal_config);
    elle::system::Process p{std::move(pc), "bin/heartbeat-server", {"--port=9090"}};
    heartbeat::run();
    p.interrupt();
    p.wait();
  };
  reactor::Thread t(sched, "test", fn);
  sched.run();
}
