#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <elle/Exception.hh>
#include <elle/os/path.hh>
#include <elle/system/home_directory.hh>

#include <common/common.hh>

#include <surface/gap/gap.hh>
#include <surface/gap/PeerTransaction.hh>
#include <surface/gap/State.hh>

#include <version.hh>

#ifdef __linux__
#include <signal.h>
#endif

ELLE_LOG_COMPONENT("8send");

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

    reactor::Thread main(
      sched,
      "send",
      [&]
      {
        common::infinit::Configuration config(production);
        surface::gap::State state(config.meta_protocol(),
                                  config.meta_host(),
                                  config.meta_port(),
                                  config.device_id(),
                                  config.trophonius_fingerprint(),
                                  config.download_dir(),
                                  common::metrics(config));
        std::vector<std::string> files;
        boost::algorithm::split(files, file, boost::is_any_of(","));
        uint32_t id;
        state.attach_callback<surface::gap::PeerTransaction>(
          [&] (surface::gap::PeerTransaction const& notif)
          {
            if (notif.id != id)
            {
              ELLE_DEBUG("received notification for another transaction: %s",
                         notif);
              return;
            }
            ELLE_TRACE_SCOPE("received notification: %s", notif);
            auto& txn = state.transactions().at(id);
            if (contains(surface::gap::Transaction::sender_final_statuses,
                         txn->data()->status))
            {
              txn->join();
              ELLE_TRACE_SCOPE("transaction is finished");
              stop = true;
            }
          });
        state.login(user, password);
        id = state.send_files(to, std::move(files), "");
        static const int width = 70;
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
      });
    sched.run();
  }
  catch (std::runtime_error const& e)
  {
    std::cerr << argv[0] << ": " << e.what() << "." << std::endl;
    return 1;
  }
}
