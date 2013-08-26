#include <iostream>
#include <stdexcept>

#include <boost/program_options.hpp>

#include <reactor/scheduler.hh>
#include <reactor/sleep.hh>
#include <elle/Exception.hh>
#include <elle/assert.hh>

#include <lune/Lune.hh>
#include <surface/gap/gap.h>
#include <surface/gap/State.hh>
#include <version.hh>

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
    ("password,p", value<std::string>(), "the password");

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
  try
  {
    auto options = parse_options(argc, argv);
    std::string const user = options["user"].as<std::string>();
    std::string const password = options["password"].as<std::string>();
    lune::Lune::Initialize(); //XXX

    reactor::Scheduler sched;

    reactor::VThread<int> t
    {
      sched,
      "recv",
      [&] () -> int
      {
        surface::gap::State state;
        bool stop = false;

        state.attach_callback<surface::gap::State::UserStatusNotification>(
          [&] (surface::gap::State::UserStatusNotification const& notif)
          {
            std::cerr << "UserStatusNotification(" << notif.id
                      << ", status: " << notif.status << std::endl;

          });

        state.attach_callback<surface::gap::TransferMachine::Notification>(
          [&] (surface::gap::TransferMachine::Notification const& notif)
          {
            std::cerr << "TransactionNotification(" << notif.id
                      << ", status: " << notif.status << std::endl;
            if (notif.status == TransferState_RecipientWaitForDecision)
            {
              state.transactions().at(notif.id).accept();
            }
            else if (notif.status == TransferState_Finished)
            {
              std::cerr << "finished" << std::endl;
              state.transactions().at(notif.id).join();
            }

          });

        auto hashed_password = state.hash_password(user, password);
        state.login(user, hashed_password);

        do
        {
          state.poll();
          reactor::Sleep s{sched, boost::posix_time::seconds{1}};
          s.run();
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
