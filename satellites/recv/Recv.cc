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
#include <elle/os/path.hh>
#include <elle/system/home_directory.hh>

#include <reactor/scheduler.hh>

#include <CrashReporter.hh>
#include <common/common.hh>
#include <surface/gap/State.hh>
#include <version.hh>

ELLE_LOG_COMPONENT("8recv");

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
    ("user,u", value<std::string>(), "the username")
    ("password,p", value<std::string>(), "the password")
    ("reject,n", value<bool>(), "Reject all incoming transfers")
    ("production,r", value<bool>(), "send metrics to production")
    ("output,o", value<std::string>(),
     "output directory (default: ~/Downloads");

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
    bool reject = false;
    if (options.count("reject") != 0)
      reject = options["reject"].as<bool>();
    std::string download_dir =
      elle::os::path::join(elle::system::home_directory().string(),
                           "Downloads");
    if (options.count("output") != 0)
      download_dir = options["output"].as<std::string>();

    if (!boost::filesystem::exists(download_dir) ||
        !boost::filesystem::is_directory(download_dir))
    {
      throw elle::Exception(
        elle::sprintf("Download directory (%s) must exist and be a directory",
                      download_dir));
    }

    reactor::Scheduler sched;

    reactor::VThread<int> t
    {
      sched,
      "recv",
      [&] () -> int
      {
        common::infinit::Configuration config(production,
                                              boost::filesystem::path(),
                                              download_dir);
        surface::gap::State state(config.meta_protocol(),
                                  config.meta_host(),
                                  config.meta_port(),
                                  config.device_id(),
                                  config.trophonius_fingerprint(),
                                  config.download_dir(),
                                  config.home(),
                                  common::metrics(config));

        state.attach_callback<surface::gap::State::ConnectionStatus>(
          [&] (surface::gap::State::ConnectionStatus const& notif)
          {
            ELLE_TRACE("connection status notification: %s", notif);
            if (!notif.status && !notif.still_trying)
              stop = true;
          }
        );

        state.attach_callback<surface::gap::State::UserStatusNotification>(
          [&] (surface::gap::State::UserStatusNotification const& notif)
          {
            ELLE_TRACE("user status notification: %s", notif);
          });

        state.attach_callback<surface::gap::Transaction::Notification>(
          [&] (surface::gap::Transaction::Notification const& notif)
          {
            try
            {
              ELLE_TRACE_SCOPE("transaction notification: %s", notif);
              if (notif.status == gap_transaction_waiting_accept)
              {
                ELLE_LOG("accept transaction %s", notif.id);
                if (reject)
                  state.transactions().at(notif.id)->reject();
                else
                  state.transactions().at(notif.id)->accept();
              }
              else if (notif.status == gap_transaction_finished)
              {
                ELLE_LOG("transaction %s finished", notif.id);
                state.transactions().at(notif.id)->join();
              }
            }
            catch(...)
            {
              ELLE_WARN("Exception while processing notification: %s",
                elle::exception_string());
            }

          });

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
