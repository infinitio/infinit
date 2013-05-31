#include <elle/log.hh>
#include <elle/Buffer.hh>
#include <elle/Exception.hh>
#include <elle/serialize/BinaryArchive.hh>

#include <reactor/thread.hh>
#include <reactor/scheduler.hh>
#include <reactor/network/udt-server.hh>
#include <reactor/network/buffer.hh>
#include <reactor/network/exception.hh>
#include <reactor/Scope.hh>

#include <satellites/satellite.hh>

#include <sstream>
#include <algorithm>
#include <memory>

ELLE_LOG_COMPONENT("oracle.disciple.heartbite");

namespace network = reactor::network;

namespace heartbite {

void
start()
{
  ELLE_TRACE_FUNCTION("");
  auto& sched = *reactor::Scheduler::scheduler();

  network::UDTServer server(sched);
  reactor::Scope scope;

  server.listen(9999);
  ELLE_LOG("starting %s", server);
  for (;;)
  {
    network::UDTSocket* sockptr{
      server.accept()
    };
    ELLE_LOG("accepting connection on %s", *sockptr);
    auto client_th = [&, sockptr]
    {
      std::unique_ptr<network::UDTSocket> sock{sockptr};
      std::string msg;

      try
      {
        while (1)
        {
          msg.resize(512);
          size_t bytes = sock->read_some(network::Buffer{msg});
          msg.resize(bytes);
          sock->write(network::Buffer{msg});
        }
      }
      catch (network::ConnectionClosed const&e)
      {
        ELLE_LOG("connection closed with %s", *sock);
      }
    };
    scope.run_background(elle::sprintf("%s", *sockptr), client_th);
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

