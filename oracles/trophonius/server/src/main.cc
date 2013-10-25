#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

#include <boost/program_options.hpp>

#include <elle/Exception.hh>
#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <version.hh>

using reactor::network::TCPServer;
using reactor::network::TCPSocket;

using namespace infinit::oracles::trophonius::server;

static
boost::program_options::variables_map
parse_options(int argc, char** argv)
{
  using namespace boost::program_options;
  options_description options("Allowed options");
  options.add_options()
    ("help,h", "display this help and exit")
    ("ping-period,i", value<int>(),
     "specify the ping period in seconds (default 30)")
    ("port,p", value<int>(), "specify the port to listen on")
    ("meta_host,m", value<std::string>(), "specify the meta host to connect to")
    ("meta_port,d", value<int>(), "specify the meta port to connect to")
    ("version,v", "display version information and exit")
    ;
  variables_map vm;
  try
  {
    store(parse_command_line(argc, argv, options), vm);
    notify(vm);
  }
  catch (invalid_command_line_syntax const& e)
  {
    throw elle::Exception(elle::sprintf("command line error: %s", e.what()));
  }
  if (vm.count("help"))
  {
    std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << options;
    std::cout << std::endl;
    std::cout << "Trophonius " INFINIT_VERSION
      " Copyright (c) 2013 infinit.io All rights reserved." << std::endl;
    exit(0); // FIXME
  }
  if (vm.count("version"))
  {
    std::cout << "Trophonius " INFINIT_VERSION << std::endl;
    exit(0); // FIXME
  }
  return vm;
}

int main(int argc, char** argv)
{
  try
  {
    reactor::Scheduler s;
    auto options = parse_options(argc, argv);
    reactor::Thread main(
      s, "main",
      [&]
      {
        if (!options.count("meta_host"))
          throw std::runtime_error("meta_host argument is mandatory");
        if (!options.count("meta_port"))
          throw std::runtime_error("meta_port argument is mandatory");

        std::string meta_host = options["meta_host"].as<std::string>();
        int meta_port = options["meta_port"].as<int>();

        int port = 0;
        int ping = 30;

        if (options.count("port"))
          port = options["port"].as<int>();
        if (options.count("ping-period"))
          ping = options["ping-period"].as<int>();

        Trophonius t(
          port, meta_host, meta_port, boost::posix_time::seconds(ping));
        // Wait forever.
        main.wait(main);
      });
    s.run();
  }
  catch (std::exception const& e)
  {
    std::cerr << "fatal error: " << e.what() << std::endl;
    return 1;
  }
  catch (...)
  {
    std::cerr << "unkown fatal error" << std::endl;
    return 1;
  }
}
