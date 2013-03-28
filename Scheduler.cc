#include <Scheduler.hh>
#include <elle/Exception.hh>

// XXX[revoir ce fichier?]

namespace infinit
{
  reactor::Scheduler&
  scheduler()
  {
    auto* ptr = reactor::Scheduler::scheduler();
    if (ptr != nullptr)
        return *ptr;
    else
        throw elle::Exception{"not running in a scheduler"};
  }
}
