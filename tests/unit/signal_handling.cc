#define BOOST_TEST_MODULE signal_handler
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <CrashReporter.hh>

#include <iostream>
#include <csignal>

#include <CrashReporter.hh>
#include <reactor/scheduler.hh>
#include <reactor/sleep.hh>

BOOST_AUTO_TEST_CASE(construction_destruction)
{
  {
    reactor::Scheduler sched;
    elle::signal::ScopedGuard guard{sched, {SIGSEGV}, [] (int sig) {}};
  }
  {
    elle::signal::ScopedGuard guard{{SIGSEGV}, [] (int sig) {}};
  }
}

// XXX: This can't work cause the synchronous guard attachs itself to the signal
// using ::signal(), checking if it replaces the default one (to prevent the
// user from erasing a handler).
// Boost test case also attachs handler to the signals, so the previous check
// will fail.
// BOOST_AUTO_TEST_CASE(synchronous)
// {
//   elle::signal::ScopedGuard guard{
//     {SIGSEGV},
//     [] (int sig) { BOOST_CHECK_EQUAL(sig, SIGSEGV); }};
//   ::kill(::getpid(), SIGSEGV);
// }

BOOST_AUTO_TEST_CASE(asynchronous)
{
  static reactor::Scheduler sched;

  bool worked = false;
  reactor::Thread asynchronous(
    sched,
    "asynchronous",
    [&]
    {
      elle::signal::ScopedGuard s{sched, {SIGSEGV},
          [&] (int)
          {
            worked = true;
        }
      };

      // Will stall.
      {
        typedef int (*crasher_t)(int);
        //((crasher_t) ((void *)(0x8000)))(0);
      }
      // Will work.
      {
        ::kill(::getpid(), SIGSEGV);
      }
      reactor::Sleep sec{sched, boost::posix_time::seconds(1)};
      sec.run();
    });

    sched.run();

    BOOST_CHECK_EQUAL(worked, true);
}
