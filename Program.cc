#include <elle/system/platform.hh>

#include <reactor/scheduler.hh>
#include <reactor/signal.hh>
#include <reactor/thread.hh>
#include <signal.h>

#include <elle/log.hh>

#include <CrashReporter.hh>
#include <Program.hh>

ELLE_LOG_COMPONENT("elle.concurrency.Program");

namespace elle
{
  namespace concurrency
  {

    std::string Program::_name("");

//
// ---------- static methods --------------------------------------------------
//


    ///
    /// this method sets up the program for startup.
    ///
    Status
    Program::Setup(std::string const& name)
    {
      Program::_name = name;
      return Status::Ok;
    }

    void
    Program::Exit()
    {
      ELLE_TRACE_SCOPE("Exit");
      _exit.signal();
    }

    void
    Program::Launch()
    {
      ELLE_TRACE_SCOPE("Launch");

      reactor::Scheduler::scheduler()->current()->wait(_exit);
    }

//
// ---------- signals ---------------------------------------------------------
//

    reactor::Signal Program::_exit;
  }
}
