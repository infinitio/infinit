#include <iostream>
#include <stdexcept>

#ifdef __linux__
#include <signal.h>
#endif

#include <boost/program_options.hpp>

#include <elle/Exception.hh>
#include <elle/assert.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>

#include <reactor/scheduler.hh>

#include <CrashReporter.hh>
#include <common/common.hh>
#include <surface/gap/State.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("8online");

bool stop = false;

static
void
interrupt(int)
{
  stop = true;
}

static
void
mandatory(boost::program_options::variables_map const& options,
          std::string const& option)
{
  if (!options.count(option))
    throw elle::Exception(
      elle::sprintf("missing mandatory option: %s", option));
}

static
boost::program_options::variables_map
parse_options(int argc, char** argv)
{
  using namespace boost::program_options;
  options_description options("Allowed options");
  options.add_options()
    ("help,h", "display the help")
    ("user,u", value<std::string>(), "the username(email)")
    ("password,p", value<std::string>(), "the password")
    ("fullname,f", value<std::string>(), "full user name")
    ("register,g", value<bool>(), "Register new account")
    ("production,r", value<bool>(), "send metrics to production");

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
    std::cout << "Infinit " INFINIT_VERSION
      " Copyright (c) 2013 infinit.io All rights reserved." << std::endl;
    exit(0); // XXX: use Exit exception
  }

  mandatory(vm, "user");
  mandatory(vm, "password");

  return vm;
}

int main(int argc, char** argv)
{
#ifdef __linux__
  signal(SIGPIPE, SIG_IGN);
#endif

  elle::signal::ScopedGuard p({SIGINT}, interrupt);

  try
  {
    auto options = parse_options(argc, argv);
    std::string const user = options["user"].as<std::string>();
    std::string const password = options["password"].as<std::string>();
    bool production = false;
    if (options.count("production") != 0)
      production = options["production"].as<bool>();

    std::string fullname;
    if (options.count("fullname") != 0)
      fullname = options["fullname"].as<std::string>();
    bool register_ = false;
    if (options.count("register") != 0)
      register_ = options["register"].as<bool>();

    reactor::Scheduler sched;

    reactor::VThread<int> t
    {
      sched,
      "online",
      [&] () -> int
      {
        common::infinit::Configuration config(production);
        surface::gap::State state(config);

        state.attach_callback<surface::gap::State::ConnectionStatus>(
          [&] (surface::gap::State::ConnectionStatus const& notif)
          {
            ELLE_TRACE_SCOPE("connection status notification: %s", notif);
            if (!notif.status && !notif.still_trying)
              stop = true;
          }
        );

        state.attach_callback<surface::gap::State::UserStatusNotification>(
          [&] (surface::gap::State::UserStatusNotification const& notif)
          {
            ELLE_TRACE_SCOPE("user status notification: %s", notif);
          });

        state.attach_callback<surface::gap::PeerTransaction>(
          [&] (surface::gap::PeerTransaction const& transaction)
          {
            ELLE_TRACE_SCOPE("transaction notification: %s", transaction);
          });

        if (register_)
          state.register_(fullname, user, password);
        else
          state.login(user, password);

        do
        {
          state.poll();
          reactor::sleep(1_sec);
        }
        while (stop != true);

        return 0;
      }
    };

    sched.run();
  }
  catch (std::runtime_error const& e)
  {
    std::cerr << argv[0] << ": " << e.what() << "." << std::endl;
    return 1;
  }
  return 0;
}
