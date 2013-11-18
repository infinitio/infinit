#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <elle/Exception.hh>
#include <elle/log.hh>
#include <elle/log/SysLogger.hh>

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
    ("meta,m", value<std::string>(),
     "specify the meta host[:port] to connect to")
    ("ignore-meta",
     "make meta registration errors non fatal")
    ("notifications-port,n", value<int>(),
     "specify the port to listen on for notifications from meta")
    ("syslog,s", "send logs to the system logger")
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
  if (vm.count("syslog"))
  {
    elle::log::logger(std::unique_ptr<elle::log::Logger>(
                        new elle::log::SysLogger("trophonius")));
  }
  return vm;
}

int main(int argc, char** argv)
{
  try
  {
    auto options = parse_options(argc, argv);
    bool meta_fatal = true;
    std::string meta_host = "";
    int meta_port = 80;
    if (!options.count("meta"))
      throw std::runtime_error("meta argument is mandatory");
    else
    {
      std::string meta = options["meta"].as<std::string>();
      std::vector<std::string> result;
      boost::split(result, meta, boost::is_any_of(":"));
      if (result.size() > 2)
        throw std::runtime_error("meta must be <host>(:<port>)");
      else if (result.size() == 2)
      {
        meta_port = std::stoi(result[1]);
        meta_host = result[0];
      }
      else
        meta_host = meta;
      if (meta_host.empty())
        throw std::runtime_error("meta host is empty");
    }
    if (options.count("ignore-meta"))
      meta_fatal = false;
    int port = 0;
    int notifications_port = 0;
    int ping = 30;
    if (options.count("port"))
      port = options["port"].as<int>();
    if (options.count("notifications-port"))
      notifications_port = options["notifications-port"].as<int>();
    if (options.count("ping-period"))
      ping = options["ping-period"].as<int>();
    reactor::Scheduler s;
    std::unique_ptr<Trophonius> trophonius;
    reactor::Thread main(
      s, "main",
      [&]
      {
        trophonius.reset(
          new Trophonius(
            port,
            meta_host,
            meta_port,
            notifications_port,
            boost::posix_time::seconds(ping),
            boost::posix_time::seconds(60),
            meta_fatal));
        // Wait for trophonius to be asked to finish.
        main.wait(*trophonius);
        trophonius.reset();
      });
    s.signal_handle(
      SIGINT,
      [&s, &trophonius]
      {
        if (trophonius)
          trophonius->stop();
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
