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

#include <boost/program_options.hpp>

ELLE_LOG_COMPONENT("oracle.disciple.heartbeat");

namespace network = reactor::network;

namespace heartbeat
{

void
start(int port)
{
  ELLE_TRACE_FUNCTION("");
  auto& sched = *reactor::Scheduler::scheduler();

  network::UDTServer server(sched);
  reactor::Scope scope;

  server.listen(port);
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

} /* heartbeat */

int
main(int ac, const char *av[])
{
  namespace po = boost::program_options;
  po::options_description cli("Heartbeat options");
  cli.add_options()
    ("help", "help message")
    ("port", po::value<int>()->default_value(9998), "port")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, cli), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cout << cli << std::endl;
    return 0;
  }

  auto Main = [&]
  {
    heartbeat::start(vm["port"].as<int>());
  };
  return infinit::satellite_main("heartbeat", std::move(Main));
}

