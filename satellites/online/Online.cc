#include <iostream>
#include <stdexcept>

#ifdef __linux__
#include <signal.h>
#endif

#include <boost/program_options.hpp>
#define BOOST_CHECK_EQUAL(a, b) ELLE_ASSERT_EQ(a, b)
#define BOOST_CHECK(a) ELLE_ASSERT(a)
#include <elle/Exception.hh>
#include <elle/assert.hh>
#include <elle/log.hh>
#include <elle/os/environ.hh>

#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

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
    ("production,r", value<bool>(), "send metrics to production")
    ("check", value<bool>(), "run a self test diagnostic");

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

static
void
exception_yield_pattern(std::vector<unsigned int> yield_pattern,
                        std::vector<bool> enable_pattern,
                        std::vector<unsigned int> no_exception)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& s)
    {
      if (enable_pattern[0])
        s.run_background("t1", [&] {
            /* Workaround compiler bug when using captures in catch(...) block
             * Symptom: error: '...' handler must be the last handler for its
             * try block [-fpermissive]
            */
            unsigned yield_count = yield_pattern[0];
            try
            {
              throw std::runtime_error("t1");
            }
            catch(...)
            {
              for (unsigned i=0; i < yield_count; ++i)
                reactor::yield();
              BOOST_CHECK_EQUAL(elle::exception_string(), "t1");
            }
        });
      if (enable_pattern[1])
        s.run_background("t2", [&] {
            unsigned yield_count = yield_pattern[1];
            try
            {
              throw std::runtime_error("t2");
            }
            catch(...)
            {
              for (unsigned i=0; i < yield_count; ++i)
                reactor::yield();
              BOOST_CHECK_EQUAL(elle::exception_string(), "t2");
            }
        });
      if (enable_pattern[2])
        s.run_background("t3", [&] {
            unsigned yield_count = yield_pattern[2];
            try
            {
              throw std::runtime_error("t3");
            }
            catch(...)
            {
              for (unsigned i=0; i < yield_count; ++i)
                reactor::yield();
              try
              {
                throw;
              }
              catch(...)
              {
                BOOST_CHECK_EQUAL(elle::exception_string(), "t3");
              }
            }
        });
      if (enable_pattern[3])
        s.run_background("t4", [&] {
            unsigned yield_count = yield_pattern[3];
            try
            {
              try
              {
                throw std::runtime_error("t4");
              }
              catch(...)
              {
                for (unsigned i=0; i<yield_count; ++i)
                  reactor::yield();
                throw;
              }
            }
            catch(...)
            {
              BOOST_CHECK_EQUAL(elle::exception_string(), "t4");
            }
        });
      // check that current_exception stays empty on non-throwing threads
      auto no_exception_test = [&](unsigned int count) {
        if (!!std::current_exception())
            ELLE_ERR("Exception in no_exception thread: %s", elle::exception_string());
        BOOST_CHECK(!std::current_exception());
        for (unsigned i=0; i<count; ++i)
        {
          reactor::yield();
          if (!!std::current_exception())
            ELLE_ERR("Exception in no_exception thread: %s", elle::exception_string());
          BOOST_CHECK(!std::current_exception());
        }
      };
      for (unsigned i=0; i <no_exception.size(); ++i)
        s.run_background("tcheck",
          [&no_exception_test, &no_exception, i] {no_exception_test(no_exception[i]);});
      s.wait();
    };
}

static void test_exception()
{
  exception_yield_pattern({ 2, 1, 2, 1}, {true, true, true, true}, {1,3});
  exception_yield_pattern({ 2, 1, 2, 1}, {true, true, true, true}, {});
  exception_yield_pattern({ 1, 2, 3, 4}, {true, true, true, true}, {});
  exception_yield_pattern({ 4, 3, 2, 1}, {true, true, true, true}, {});
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
        surface::gap::State state(config.meta_protocol(),
                                  config.meta_host(),
                                  config.meta_port(),
                                  config.device_id(),
                                  config.trophonius_fingerprint(),
                                  config.download_dir(),
                                  common::metrics(config));

        state.attach_callback<surface::gap::State::ConnectionStatus>(
          [&] (surface::gap::State::ConnectionStatus const& notif)
          {
            ELLE_TRACE_SCOPE("connection status notification: %s", notif);
            if (!notif.status && !notif.still_trying)
              stop = true;
            if (options.count("check"))
            {
              ELLE_LOG("Running coro check...")
              test_exception();
              ELLE_LOG("...check PASSED");
            }
          }
        );

        state.attach_callback<surface::gap::State::UserStatusNotification>(
          [&] (surface::gap::State::UserStatusNotification const& notif)
          {
            ELLE_TRACE_SCOPE("user status notification: %s", notif);
          });

        state.attach_callback<surface::gap::Transaction::Notification>(
          [&] (surface::gap::Transaction::Notification const& notif)
          {
            ELLE_TRACE_SCOPE("transaction notification: %s", notif);
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
