#include <iostream>
#include <stdexcept>

#include <boost/program_options.hpp>

#include <reactor/scheduler.hh>
#include <reactor/sleep.hh>
#include <elle/Exception.hh>

#include <CrashReporter.hh>

#include <lune/Lune.hh>
#include <surface/gap/gap.h>
#include <surface/gap/State.hh>
#include <version.hh>

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
    ("to,t", value<std::string>(), "the recipient")
    ("file,f", value<std::string>(), "the file to send");

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
  mandatory(vm, "to");
  mandatory(vm, "file");

  return vm;
}

int main(int argc, char** argv)
{
  /// Initialize with signal list and handler (sync).
  elle::signal::ScopedGuard p({SIGINT}, interrupt);

  try
  {
    auto options = parse_options(argc, argv);
    std::string const user = options["user"].as<std::string>();
    std::string const password = options["password"].as<std::string>();
    std::string const to = options["to"].as<std::string>();
    std::string const file = options["file"].as<std::string>();

    lune::Lune::Initialize(); // XXX

    reactor::Scheduler sched;

    reactor::VThread<int> t
    {
      sched,
      "sendto",
      [&] () -> int
      {
        surface::gap::State state;
        uint32_t id = surface::gap::null_id;

        state.attach_callback<surface::gap::State::ConnectionStatus>(
          [&] (surface::gap::State::ConnectionStatus const& notif)
          {
            std::cerr << "ConnectionStatusNotification(" << notif.status
                      << ")" << std::endl;
          }
        );

        state.attach_callback<surface::gap::State::TrophoniusUnavailable>(
          [&]
          (surface::gap::State::TrophoniusUnavailable const& notif)
          {
            std::cerr << "TrophoniusUnavailable" << std::endl;
          }
        );

        state.attach_callback<surface::gap::State::KickedOut>(
          [&]
          (surface::gap::State::KickedOut const& notif)
          {
            std::cerr << "KickedNotification" << std::endl;
          }
        );

        state.attach_callback<surface::gap::State::UserStatusNotification>(
          [&] (surface::gap::State::UserStatusNotification const& notif)
          {
          });

        state.attach_callback<surface::gap::Transaction::Notification>(
          [&] (surface::gap::Transaction::Notification const& notif)
          {
            if (id == surface::gap::null_id)
              return;

            if (notif.id != id)
              return;

            if (notif.status == gap_transaction_finished)
            {
              state.transactions().at(id).join();
              stop = true;
            }
          });
        auto hashed_password = state.hash_password(user, password);
        state.login(user, hashed_password);
        // state.update_device("lust");

        id = state.send_files(to, {file.c_str()}, "");

        if (id == surface::gap::null_id)
          throw elle::Exception("transaction id is null");

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

    // bool stop = false;
    // std::string transaction_id;
    // state.notification_manager().transaction_callback(
    //   [&] (plasma::trophonius::TransactionNotification const& t, bool)
    //   {
    //     if (state.transaction_id(mid) != t.id)
    //       return;

    //     transaction_id = t.id;

    //     if (t.status == plasma::TransactionStatus::finished ||
    //         t.status == plasma::TransactionStatus::canceled)
    //       stop = true;
    //   });

    // while (!stop)
    //   state.notification_manager().poll(1);

    // state.join_transaction(transaction_id);
  }
  catch (std::runtime_error const& e)
  {
    std::cerr << argv[0] << ": " << e.what() << "." << std::endl;
    return 1;
  }

  return 0;
}
