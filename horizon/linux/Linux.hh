#ifndef HORIZON_LINUX_LINUX_HH
# define HORIZON_LINUX_LINUX_HH

# include <elle/types.hh>

# include <reactor/scheduler.hh>

namespace horizon
{
  ///
  /// this namespace contains all the elements related to the Linux horizon i.e
  /// interface between the kernel and Infinit.
  ///
  namespace linux
  {

    ///
    /// this class contains the general-purpose information related to the
    /// Linux horizon.
    ///
    class Linux
    {
    public:
      //
      // static methods
      //
      static
      void
      Initialize(reactor::Scheduler& sched);
      static elle::Status       Clean();
    };

  }
}

#endif
