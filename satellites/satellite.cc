#include <reactor/scheduler.hh>
#include <reactor/exception.hh>

#include <elle/system/signal.hh>
#include <elle/system/Process.hh>
#include <elle/log.hh>
#include <elle/log/TextLogger.hh>
#include <elle/os/getenv.hh>
#include <common/common.hh>
#include <satellites/satellite.hh>
#include <CrashReporter.hh>

#include <unistd.h>
#include <sys/wait.h>

#include <fstream>
#include <sstream>
#include <cerrno>

ELLE_LOG_COMPONENT("infinit.satellite");

static
std::ostream&
log_destination()
{
  if (auto env = ::getenv("INFINIT_LOG_FILE"))
    {
      static std::ofstream res(env, std::fstream::trunc | std::fstream::out);
      return res;
    }
  else
    return std::cerr;
}

namespace infinit
{
  static int st_pid = -1;

  void
  sighdl(int signum)
  {
    ELLE_DEBUG_SCOPE("received signal %s(%d)",
                     elle::system::strsignal(signum), signum);
    if (st_pid != -1)
    {
      ELLE_DEBUG("kill(%d, %s(%d))",
                 st_pid, elle::system::strsignal(signum), signum);
      kill(st_pid, signum);
    }
  }

  static
  int
  _satellite_trace(int pid, std::string const& name)
  {
    ELLE_LOG_COMPONENT("infinit.satellite.parent");
    int err;
    int status;
    int retval = -1;

    st_pid = pid;
    signal(SIGINT, sighdl);
    signal(SIGTERM, sighdl);
    signal(SIGQUIT, sighdl);

    ELLE_DEBUG("%s[%d]: start tracing", name, pid);
    while ((err = waitpid(pid, &status, 0)) != pid)
    {
      if (errno == EINTR)
        continue;
      int _errno = errno; // Hack for context switch in throw stmt.
      throw elle::Exception{elle::sprintf("waitpid: error %s: %s",
                                          err, ::strerror(_errno))};
    }
    ELLE_DEBUG("finished waiting %s", pid);
    if (WIFEXITED(status))
    {
      retval = WEXITSTATUS(status);
      ELLE_LOG("%s[%d]: exited with status %d", name, pid, retval);
    }
    if (WIFSIGNALED(status) || WIFSTOPPED(status))
    {
      std::stringstream ss;
      int signum = WTERMSIG(status);
      ELLE_ERR("%s[%d]: stopped by signal %s(%d)", name, pid,
               elle::system::strsignal(signum), signum);
      retval = 128 + signum;
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
                                     "-x", common::infinit::binary_path("gdbmacro.py"));
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
    auto sched = reactor::Scheduler::scheduler();
    try
    {
      auto sig_fn = [sched] (int)
      {
        auto sched = reactor::Scheduler::scheduler();
        if (sched != nullptr)
            sched->terminate();
        else
            ELLE_WARN("signal caught, but no scheduler alive");
      };
      elle::signal::ScopedGuard sigint{*sched, {SIGINT}, sig_fn};
      action();
      ELLE_DEBUG("quiting %s", name);
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
    elle::log::logger
      (std::unique_ptr<elle::log::Logger>
       (new elle::log::TextLogger(log_destination())));

    ELLE_TRACE_FUNCTION(name, action);

    int pid = 0;
    std::string shallfork = elle::os::getenv("INFINIT_NO_FORK", "");
    if (shallfork.empty() && (pid = fork()))
    {
      return _satellite_trace(pid, name);
    }
    else
    {
      ELLE_LOG_COMPONENT("infinit.satellite.child");
      reactor::Scheduler sched;
      try
      {
        reactor::VThread<int> main(sched, name, std::bind(_satellite_wrapper,
                                                          name, action));
        sched.run();
        return main.result();
      }
      catch (reactor::Exception const& e)
      {
        ELLE_ERR("%s: fatal error: %s", name, e);
        std::cerr << name << ": fatal error: " << e << std::endl;
        elle::crash::report(common::meta::host(),common::meta::port(),
                            name, e.what());
        return 1;
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
