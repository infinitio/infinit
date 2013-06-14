#include <elle/log.hh>
#include <elle/Buffer.hh>
#include <elle/Exception.hh>
#include <elle/serialize/BinaryArchive.hh>

#include <reactor/thread.hh>
#include <reactor/scheduler.hh>
#include <reactor/network/udt-socket.hh>
#include <reactor/network/buffer.hh>
#include <reactor/sleep.hh>

#include <satellites/satellite.hh>

#include <sstream>
#include <algorithm>
#include <memory>

ELLE_LOG_COMPONENT("oracle.disciple.heartbite");

namespace network = reactor::network;

namespace heartbite {

void
sleep(int seconds)
{
  auto& sched = *reactor::Scheduler::scheduler();
  reactor::Sleep s{sched, boost::posix_time::seconds{seconds}};
  s.run();
}

void
start()
{
  auto& sched = *reactor::Scheduler::scheduler();

  ELLE_LOG("connecting to localhost");
  network::UDTSocket socket(sched, "127.0.0.1", 9999);

  for (int i = 0; i < 10; ++i)
  {
    std::string msg{"echo"};
    socket.write(network::Buffer{msg});
    msg.resize(512);
    size_t bytes = socket.read_some(network::Buffer{msg});
    msg.resize(bytes);
    ELLE_DUMP("received %s", msg);
    sleep(10);
  }
}

} /* heartbite */

int
main(int, const char *[])
{
  auto Main = [&]
  {
    heartbite::start();
  };
  return infinit::satellite_main("heartbite", std::move(Main));
}

