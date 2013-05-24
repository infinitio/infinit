#include <horizon/linux/FUSE.hh>
#include <horizon/linux/FUker.hh>

#include <elle/Exception.hh>

namespace horizon
{
  namespace linux
  {

//
// ---------- definitions -----------------------------------------------------
//

    ///
    /// this structure contains the function pointers to the FUSE
    /// implementation routines located in the crux.
    ///
    struct ::fuse_operations            FUSE::Operations;

//
// ---------- methods ---------------------------------------------------------
//

    void
    FUSE::Initialize(reactor::Scheduler& sched,
                     struct ::fuse_operations const& operations)
    {
      // set the fuse operations.
      FUSE::Operations = operations;
      // initialize the FUker.
      FUker::Initialize(sched);
    }

    ///
    /// this method cleans the system module.
    ///
    elle::Status        FUSE::Clean()
    {
      // clean the FUker.
      if (FUker::Clean() == elle::Status::Error)
        throw elle::Exception("unable to clean the FUker");

      return elle::Status::Ok;
    }

  }
}
