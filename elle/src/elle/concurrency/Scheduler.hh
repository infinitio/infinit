#ifndef ELLE_CONCURRENCY_SCHEDULER_HH
# define ELLE_CONCURRENCY_SCHEDULER_HH

# include <reactor/scheduler.hh>

// XXX[revoir ce fichier?]

namespace elle
{
  namespace concurrency
  {
    reactor::Scheduler& scheduler();
  }
}

#endif
