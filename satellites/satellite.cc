#include <reactor/scheduler.hh>

#include <CrashReporter.hh>
#include <common/common.hh>
#include <satellites/satellite.hh>

namespace infinit
{
  static
  int
  _satellite_wrapper(std::string const& name,
                     std::function<void ()> const& action)
  {
    try
      {
        // Capture signal and send email without exiting.
        elle::signal::ScopedGuard guard
          (*reactor::Scheduler::scheduler(),
           {SIGINT, SIGABRT, SIGPIPE, SIGTERM},
           elle::crash::Handler(common::meta::host(),
                                common::meta::port(),
                                name, false));

        // Capture signal and send email exiting.
        elle::signal::ScopedGuard exit_guard
          (*reactor::Scheduler::scheduler(),
           {SIGILL, SIGSEGV},
           elle::crash::Handler(common::meta::host(),
                                common::meta::port(),
                                name, true));

        action();
        return 0;
      }
    catch (std::runtime_error const& e)
      {
        std::cerr << name << ": fatal error: " << e.what() << std::endl;
        elle::crash::report(common::meta::host(), common::meta::port(),
                            name, e.what());
        return 1;
      }
  }

  int
  satellite_main(std::string const& name, std::function<void ()> const& action)
  {
    reactor::Scheduler sched;

    try
      {
        reactor::VThread<int> main(sched, name, std::bind(_satellite_wrapper,
                                                          name, action));
        sched.run();
        return main.result();
      }
    catch (std::runtime_error const& e)
      {
        std::cerr << name << ": fatal error: " << e.what() << std::endl;
        elle::crash::report(common::meta::host(), common::meta::port(),
                            name, e.what());
        return 1;
      }
  }

}
