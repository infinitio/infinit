#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <elle/Exception.hh>
#include <elle/log/SysLogger.hh>
#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/apertus/Apertus.hh>
#include <version.hh>


static
boost::program_options::variables_map
parse_options(int argc, char** argv)
{
  using namespace boost::program_options;
  options_description options("Allowed options");
  options.add_options()
    ("help,h", "display this help and exit")
    ("port-ssl,P", value<int>(), "specify the SSL port to listen on")
    ("port-tcp,p", value<int>(), "specify the TCP port to listen on")
    ("meta,m", value<std::string>(),
     "specify the meta protocol://host[:port] to connect to")
    ("tick,t", value<long>(), "specify the rate at which to announce the load to meta")
    ("client-timeout", value<int>(), "specify timeout in seconds after which non-paired clients are disconnected (300sec)")
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
    std::cout << "Apertus " INFINIT_VERSION
      " Copyright (c) 2013 infinit.io All rights reserved." << std::endl;
    exit(0); // FIXME
  }
  if (vm.count("version"))
  {
    std::cout << "Apertus " INFINIT_VERSION << std::endl;
    exit(0); // FIXME
  }
  if (vm.count("syslog"))
  {
    elle::log::logger(std::unique_ptr<elle::log::Logger>(
                        new elle::log::SysLogger("apertus")));
  }
  return vm;
}

int main(int argc, char** argv)
{
  try
  {
    auto options = parse_options(argc, argv);


    std::string meta_protocol = "http";
    std::string meta_host = "";
    int meta_port = 80;

    if (options.count("meta"))
    {
      std::string meta = options["meta"].as<std::string>();
      std::vector<std::string> result;
      boost::split(result, meta, boost::is_any_of(":"));
      if (result.size() > 3)
        throw std::runtime_error("meta must be <host>(:<port>)");
      else if (result.size() == 3)
      {
        meta_protocol = result[0];
        meta_host = result[1];
        // Remove slashes after protocol.
        meta_host.erase(std::remove(meta_host.begin(), meta_host.end(), '/'),
                        meta_host.end());
        meta_port = std::stoi(result[2]);
      }
      else
      {
        meta_host = meta;
      }
    }

    int port_ssl = 0;
    int port_tcp = 0;
    auto tick = 10_sec;
    auto client_timeout = 300_sec;

    if (options.count("port-ssl"))
      port_ssl = options["port-ssl"].as<int>();
    if (options.count("port-tcp"))
      port_tcp = options["port-tcp"].as<int>();
    if (options.count("tick"))
      tick = boost::posix_time::seconds(options["tick"].as<long>());
   if (options.count("client-timeout"))
      client_timeout = boost::posix_time::seconds(options["client-timeout"].as<int>());

    reactor::Scheduler sched;

    std::unique_ptr<infinit::oracles::apertus::Apertus> apertus;

    reactor::Thread main(sched, "main", [&]
      {
        apertus.reset(
          new infinit::oracles::apertus::Apertus(
            meta_protocol,
            meta_host,
            meta_port,
            "0.0.0.0",
            port_ssl,
            port_tcp,
            tick,
            client_timeout));

        reactor::wait(*apertus);
        apertus.reset();
      });

    sched.signal_handle(
      SIGINT,
      [&]
      {
        if (apertus)
          apertus->stop();
      });

    sched.run();
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
