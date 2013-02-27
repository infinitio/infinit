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
    std::string Program::_host("");
    int Program::_port(0);

//
// ---------- static methods --------------------------------------------------
//


    ///
    /// this method sets up the program for startup.
    ///
    Status
    Program::Setup(std::string const& name, std::string const& host, int port)
    {
      Program::_name = name;
      Program::_host = host;
      Program::_port = port;
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
