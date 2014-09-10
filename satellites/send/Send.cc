#include <iostream>

#include <boost/program_options.hpp>

#include <elle/Exception.hh>

#include <common/common.hh>

#include <surface/gap/gap.hh>
#include <surface/gap/State.hh>

#include <version.hh>

#ifdef __linux__
#include <signal.h>
#endif

bool stop = false;

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
    ("file,f", value<std::string>(), "the file to send, or comma-separated list")
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
  mandatory(vm, "to");
  mandatory(vm, "file");

  return vm;
}

int main(int argc, char** argv)
{
#ifdef __linux__
  signal(SIGPIPE, SIG_IGN);
#endif
  try
  {
    auto options = parse_options(argc, argv);
    std::string const user = options["user"].as<std::string>();
    std::string const password = options["password"].as<std::string>();
    std::string const to = options["to"].as<std::string>();
    std::string const file = options["file"].as<std::string>();
    bool production = false;
    if (options.count("production") != 0)
      production = options["production"].as<bool>();

    reactor::Scheduler sched;

    sched.signal_handle(
      SIGINT,
      [&sched]
      {
        sched.terminate();
      });

    reactor::VThread<int> t
    {
      sched,
      "sendto",
      [&] () -> int
      {
        common::infinit::Configuration config(production);
        surface::gap::State state(config.meta_protocol(),
                                  config.meta_host(),
                                  config.meta_port(),
                                  config.device_id(),
                                  config.trophonius_fingerprint(),
                                  common::metrics(config));
        uint32_t id = surface::gap::null_id;

        state.attach_callback<surface::gap::State::ConnectionStatus>(
          [&] (surface::gap::State::ConnectionStatus const& notif)
          {}
        );

        state.attach_callback<surface::gap::State::TrophoniusUnavailable>(
          [&]
          (surface::gap::State::TrophoniusUnavailable const& notif)
          {}
        );

        state.attach_callback<surface::gap::State::KickedOut>(
          [&]
          (surface::gap::State::KickedOut const& notif)
          {}
        );

        state.attach_callback<surface::gap::State::UserStatusNotification>(
          [&] (surface::gap::State::UserStatusNotification const& notif)
          {});

        state.attach_callback<surface::gap::Transaction::Notification>(
          [&] (surface::gap::Transaction::Notification const& notif)
          {
            if (id == surface::gap::null_id)
              return;

            if (notif.id != id)
              return;

            auto& txn = state.transactions().at(id);

            if (surface::gap::Transaction::final_statuses.find(txn->data()->status) != surface::gap::Transaction::final_statuses.end())
            {
              txn->join();
              stop = true;
            }
          });
        auto hashed_password = state.hash_password(user, password);
        state.login(user, hashed_password);
        // state.update_device("lust");
        std::vector<std::string> files;
        boost::algorithm::split(files, file, boost::is_any_of(","));
        id = state.send_files(to, std::move(files), "");

        if (id == surface::gap::null_id)
          throw elle::Exception("transaction id is null");

        static const int width = 70;
        std::cout << std::endl;
        float previous_progress = 0.0;
        do
        {
          state.poll();
          //std::cout << "[A[J";
          float progress = 0.0;
          if (stop)
            progress = 1.0;
          else
            progress = state.transactions().at(id)->progress();
          if (progress != previous_progress)
          {
            previous_progress = progress;
            std::cout << "[";
            for (int i = 0; i < width - 2; ++i)
            {
              std::cout << (float(i) / width < progress ? "#" : " ");
            }
            std::cout << "] " << int(progress * 100) << "%" << std::endl;
          }
          reactor::sleep(100_ms);
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
