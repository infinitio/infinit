#include <reactor/scheduler.hh>

#include <elle/system/signal.hh>
#include <elle/system/Process.hh>
#include <elle/log.hh>
#include <common/common.hh>
#include <satellites/satellite.hh>
#include <CrashReporter.hh>

#include <unistd.h>
#include <sys/wait.h>

#include <fstream>
#include <sstream>

ELLE_LOG_COMPONENT("infinit.satellite");

namespace infinit
{

  static
  int
  _satellite_trace(int pid, std::string const& name)
  {
    int status;
    int retval = -1;

    ELLE_DEBUG("%s[%d]: start tracing", name, pid);
    int err;
    while ((err = waitpid(pid, &status, 0)) == -1)
    {
      if (err == EINTR)
        continue;
    }
    if (WIFEXITED(status))
    {
      retval = WEXITSTATUS(status);
      ELLE_LOG("%s[%d]: exited with status %d", name, pid, retval);
    }
    if (WIFSIGNALED(status) || WIFSTOPPED(status))
    {
      std::stringstream ss;
      int signum = WTERMSIG(status);
      ELLE_LOG("%s[%d]: stopped by signal %s(%d)", name, pid,
               elle::system::strsignal(signum), signum);
      retval = -signum;
      if (signum == SIGKILL)
        return retval;
#if defined WCOREDUMP
      if (WCOREDUMP(status))
      {
        ELLE_LOG("%s[%d]: core dumped", name, pid);
        ss << 
          elle::system::check_output("gdb",
                                     "-e", common::infinit::binary_path(name),
#if defined INFINIT_LINUX
                                     "-c", "core",
#elif defined INFINIT_MACOSX
                                     "-c", elle::sprintf("/cores/core.%d", pid),
#endif
                                     "-s", common::infinit::binary_path(name),
                                     "-x", common::infinit::binary_path("gdbmacro"));
        std::ofstream debuginfo{
          elle::sprintf("/tmp/crash-%s-%d.txt", name, pid)};
        debuginfo << ss.str();
      }
#endif
      elle::crash::report(common::meta::host(), common::meta::port(),
                          name, elle::system::strsignal(signum),
                          elle::Backtrace::current(), ss.str());
    }
    return retval;
  }

  static
  int
  _satellite_wrapper(std::string const& name,
                     std::function<void ()> const& action)
  {
    try
    {
      auto sig_fn = [] (int)
      {
        reactor::Scheduler& sched = *reactor::Scheduler::scheduler();
        sched.terminate();
      };
      elle::signal::ScopedGuard sigint{{SIGINT}, std::move(sig_fn)};

      action();
      return 0;
    }
    catch (std::runtime_error const& e)
    {
      ELLE_ERR("%s: fatal error: %s", name, e.what());
      std::cerr << name << ": fatal error: " << e.what() << std::endl;
      elle::crash::report(common::meta::host(), common::meta::port(),
                          name, e.what());
      return 1;
    }
  }

  int
  satellite_main(std::string const& name, std::function<void ()> const& action)
  {
    ELLE_TRACE_FUNCTION(name, action);

    int pid = 0;
    if ((pid = fork()))
    {
      return _satellite_trace(pid, name);
    }
    else
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
        ELLE_ERR("%s: fatal error: %s", name, e.what());
        std::cerr << name << ": fatal error: " << e.what() << std::endl;
        elle::crash::report(common::meta::host(),common::meta::port(),
                            name, e.what());
        return 1;
      }
    }
  }

}
