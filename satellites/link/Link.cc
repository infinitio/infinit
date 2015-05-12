#ifdef __linux__
#include <signal.h>
#endif

#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <elle/Exception.hh>

#include <reactor/scheduler.hh>
#include <reactor/timer.hh>

#include <CrashReporter.hh>
#include <common/common.hh>
#include <surface/gap/State.hh>
#include <surface/gap/gap.hh>
#include <version.hh>

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
    ("file,f", value<std::string>(),
     "the file to send, or comma-separated list")
    ("pause,x", value<bool>(),
     "pause transaction midway, for testing purposes")
    ("production,r", value<bool>(), "use production servers");
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
    bool production = false;
    if (options.count("production") != 0)
      production = options["production"].as<bool>();
    bool pause = (options.count("pause") != 0);

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
      "create link",
      [&] () -> int
      {
        common::infinit::Configuration config(production);
        surface::gap::State state(config);
        uint32_t id = surface::gap::null_id;
        state.attach_callback<surface::gap::LinkTransaction>(
          [&] (surface::gap::LinkTransaction const& t)
          {
            if (id == surface::gap::null_id || t.id != id)
              return;
            if (t.status == gap_transaction_finished)
            {
              state.transactions().at(id)->join();
              stop = true;
            }
          });
        state.login(user, password);
        std::string file_names = options["file"].as<std::string>();
        std::vector<std::string> files;
        boost::algorithm::split(files, file_names, boost::is_any_of(","));
        id = state.create_link(files, "");
        ELLE_ASSERT_NEQ(id, surface::gap::null_id);

        static const int width = 70;
        std::cout << std::endl;
        float previous_progress = 0.0;
        bool paused = false;
        std::unique_ptr<reactor::Timer> timer;
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
          if (pause && progress > 0.3 && !paused)
          {
            state.transaction_pause(id);
            timer = elle::make_unique<reactor::Timer>(
            "resume", boost::posix_time::seconds(5),
            [&state, id]()
            {
              state.transaction_pause(id, false);
            });
            paused = true;
          }
          reactor::sleep(100_ms);
        }
        while (!stop);
        auto link =
          std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
            state.transactions().at(id)->data());
        ELLE_ASSERT(link.get());
        std::cerr << link->share_link << std::endl;
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
}
