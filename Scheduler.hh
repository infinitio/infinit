#ifndef ELLE_CONCURRENCY_SCHEDULER_HH
# define ELLE_CONCURRENCY_SCHEDULER_HH

# include <reactor/scheduler.hh>

# include <Scheduler.hh>

// FIXME: replace this with Scheduler::scheduler()
namespace infinit
{
  reactor::Scheduler& scheduler();
}

#endif
