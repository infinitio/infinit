#include <infinit/oracles/trophonius/server/Client.hh>
#include <infinit/oracles/trophonius/server/Trophonius.hh>

#include <algorithm>

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
    ("hostname", value<std::string>(), "hostname to send to meta")
    ("ping-period,i", value<int>(),
     "specify the ping period in seconds (default 30)")
    ("auth-max-time", value<int>(),
     "specify max time between client connect and login request in seconds (default 10)")
    ("port-ssl,pssl", value<int>(), "specify the SSL port to listen on")
    ("port-tcp,ptcp", value<int>(), "specify the TCP port to listen on")
    ("meta,m",
     value<std::string>()->default_value
     ("meta." BOOST_PP_STRINGIZE(INFINIT_VERSION_MINOR) "."
      BOOST_PP_STRINGIZE(INFINIT_VERSION_MAJOR) ".api.production.infinit.io"),
     "meta host")
    ("ignore-meta",
     "make meta registration errors non fatal")
    ("notifications-port,n", value<int>(),
     "specify the port to listen on for notifications from meta")
    ("syslog,s", "send logs to the system logger")
    ("version,v", "display version information and exit")
    ("zone,z", value<std::string>(), "specify the zone to send to meta")
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
    ELLE_ASSERT(options.count("meta"));
    std::string meta = options["meta"].as<std::string>();
    bool meta_fatal = true;
    if (options.count("ignore-meta"))
      meta_fatal = false;
    int port_ssl = 0;
    int port_tcp = 0;
    int notifications_port = 0;
    int ping = 30;
    int auth_max_time = 10;
    boost::optional<std::string> hostname;
    if (options.count("port-ssl"))
      port_ssl = options["port-ssl"].as<int>();
    if (options.count("port-tcp"))
      port_tcp = options["port-tcp"].as<int>();
    if (options.count("notifications-port"))
      notifications_port = options["notifications-port"].as<int>();
    if (options.count("ping-period"))
      ping = options["ping-period"].as<int>();
    if (options.count("auth-max-time"))
      auth_max_time = options["auth-max-time"].as<int>();
    boost::optional<std::string> zone;
    if (options.count("zone"))
      zone = options["zone"].as<std::string>();
    if (options.count("hostname"))
      hostname = options["hostname"].as<std::string>();
    reactor::Scheduler s;
    std::unique_ptr<Trophonius> trophonius;
    reactor::Thread main(
      s, "main",
      [&]
      {
        trophonius.reset(
          new Trophonius(
            port_ssl,
            port_tcp,
            meta,
            notifications_port,
            boost::posix_time::seconds(ping),
            boost::posix_time::seconds(60),
            boost::posix_time::seconds(auth_max_time),
            meta_fatal,
            zone,
            hostname));
        // Wait for trophonius to be asked to finish.
        reactor::wait(*trophonius);
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
    std::cerr << "unknown fatal error" << std::endl;
    return 1;
  }
}
