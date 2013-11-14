#include <infinit/oracles/apertus/Apertus.hh>

#include <boost/program_options.hpp>

#include <elle/Exception.hh>
#include <reactor/scheduler.hh>
#include <version.hh>



static
boost::program_options::variables_map
parse_options(int argc, char** argv)
{
  using namespace boost::program_options;
  options_description options("Allowed options");
  options.add_options()
    ("help,h", "display this help and exit")
    ("port,p", value<int>(), "specify the port to listen on")
    ("meta,m", value<std::string>(),
     "specify the meta host[:port] to connect to")
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
  return vm;
}

int main(int argc, char** argv)
{
  try
  {
    auto options = parse_options(argc, argv);

    if (!options.count("meta"))
      throw std::runtime_error("meta argument is mandatory");

    std::string meta_host = "";
    int meta_port = 80;

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

    int port = 0;

    if (options.count("port"))
      port = options["port"].as<int>();

    reactor::Scheduler sched;

    std::unique_ptr<oracles::apertus::Apertus> ap;

    reactor::Thread main(sched, "main", [&]
      {
        ap.reset(new oracles::apertus::Apertus(
          sched, meta_host, meta_port, "0.0.0.0", port));

        ap->reg();

        main.wait(*ap);
        ap.reset();
      });

    sched.signal_handle(
      SIGINT,
      [&]
      {
        if (ap)
        {
          ap->unreg();
          ap->stop();
        }
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
    std::cerr << "unkown fatal error" << std::endl;
    return 1;
  }
}
