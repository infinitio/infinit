#ifndef HORIZON_LINUX_FUSE_HH
# define HORIZON_LINUX_FUSE_HH

# ifndef FUSE_USE_VERSION
#  define FUSE_USE_VERSION               26
# endif

# include <elle/types.hh>

# include <reactor/scheduler.hh>

# include <fuse/fuse.h>

namespace horizon
{
//XXX
#undef linux
  namespace linux
  {

    ///
    /// this class contains everything related to FUSE.
    ///
    class FUSE
    {
    public:
      //
      // static methods
      //
      static
      void
      Initialize(reactor::Scheduler& sched,
                 struct ::fuse_operations const&);
      static elle::Status       Clean();

      //
      // static attributes
      //
      static struct ::fuse_operations   Operations;
    };

  }
}

#endif
