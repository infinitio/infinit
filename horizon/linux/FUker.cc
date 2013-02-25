#include <elle/concurrency/Program.hh>
#include <elle/concurrency/Scheduler.hh>

#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

#include <horizon/linux/FUker.hh>
#include <horizon/linux/FUSE.hh>
#include <horizon/operations.hh>
#include <horizon/Horizon.hh>

#include <hole/Hole.hh>

#include <Infinit.hh>

#include <boost/function.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/pop_front.hpp>
#include <pthread.h>
#include <sys/param.h>
#include <sys/statfs.h>
#include <fuse/fuse_lowlevel.h>

ELLE_LOG_COMPONENT("infinit.horizon.FUker");

namespace horizon
{
  namespace linux
  {
    // XXX
    typedef struct ::timespec timespec2[2];

    ///
    /// this is the identifier of the thread which is spawn so as to start
    /// FUSE.
    ///
    /// note that creating a specific thread is required because the call
    /// to fuse_main() never returns.
    ///
    ::pthread_t                 FUker::Thread;

    ///
    /// this attribute represents the main structure for manipulating a
    /// FUSE-mounted point.
    ///
    struct ::fuse*              FUker::FUSE = nullptr;

    /// The callbacks below are triggered by FUSE whenever a kernel
    /// event occurs.
    ///
    /// Note that every one of the callbacks run in a specific thread.
    ///
    /// the purpose of the code is to create an event, inject it in
    /// the event loop so that the Broker can treat it in a fiber and
    /// possible block.
    ///
    /// since the thread must not return until the event is treated,
    /// the following relies on a semaphore by blocking on it. once
    /// the event handled, the semaphore is unlocked, in which case
    /// the thread is resumed and can terminate by returning the
    /// result of the upcall.

#define INFINIT_FUSE_FORMALS(R, Data, I, Elem)  \
    , Elem BOOST_PP_CAT(a, BOOST_PP_INC(I))

#define INFINIT_FUSE_EFFECTIVE_(R, Data, I, Elem)       \
    , BOOST_PP_CAT(a, BOOST_PP_INC(I))

#define INFINIT_FUSE_EFFECTIVE(Args)                            \
    a0 BOOST_PP_SEQ_FOR_EACH_I(INFINIT_FUSE_EFFECTIVE_, _,      \
                               BOOST_PP_SEQ_POP_FRONT(Args))    \

#define INFINIT_FUSE_BOUNCER(Name, Args)                                \
    static int                                                          \
    Name(BOOST_PP_SEQ_HEAD(Args) a0                                     \
         BOOST_PP_SEQ_FOR_EACH_I(INFINIT_FUSE_FORMALS, _,               \
                                 BOOST_PP_SEQ_POP_FRONT(Args)))         \
    {                                                                   \
      ELLE_LOG_COMPONENT("infinit.horizon.Crux");                       \
                                                                        \
      try                                                               \
        {                                                               \
          int res = FUSE::Operations.Name(INFINIT_FUSE_EFFECTIVE(Args));\
          ELLE_TRACE("%s returns: %s",                                  \
                     BOOST_PP_STRINGIZE(Name), res);                    \
          return res;                                                   \
        }                                                               \
      catch (elle::Exception const& e)                               \
        {                                                               \
          ELLE_ERR("%s killed by exception: %s",                        \
                   BOOST_PP_STRINGIZE(Name), e);                        \
          return -EIO;                                                  \
        }                                                               \
      catch (std::exception const& e)                                   \
        {                                                               \
          ELLE_ERR("%s killed by standard exception: %s",               \
                   BOOST_PP_STRINGIZE(Name), e.what());                 \
          return -EIO;                                                  \
        }                                                               \
      catch (...)                                                       \
        {                                                               \
          ELLE_ERR("%s killed by unknown exception",                    \
                   BOOST_PP_STRINGIZE(Name));                           \
          return -EIO;                                                  \
        }                                                               \
    }                                                                   \
                                                                        \
    static int                                                          \
    BOOST_PP_CAT(Name, _mt)(BOOST_PP_SEQ_HEAD(Args) a0                  \
         BOOST_PP_SEQ_FOR_EACH_I(INFINIT_FUSE_FORMALS, _,               \
                                 BOOST_PP_SEQ_POP_FRONT(Args)))         \
    {                                                                   \
      return elle::concurrency::scheduler().mt_run<int>                 \
        (BOOST_PP_STRINGIZE(Name),                                      \
         boost::bind(Name, INFINIT_FUSE_EFFECTIVE(Args)));              \
    }                                                                   \

#define INFINIT_FUSE_BOUNCER_(R, Data, Elem)                            \
    INFINIT_FUSE_BOUNCER(BOOST_PP_TUPLE_ELEM(2, 0, Elem),               \
                         BOOST_PP_TUPLE_ELEM(2, 1, Elem))               \

    BOOST_PP_SEQ_FOR_EACH(INFINIT_FUSE_BOUNCER_, _, INFINIT_FUSE_COMMANDS)

#undef INFINIT_FUSE_BOUNCER_
#undef INFINIT_FUSE_BOUNCER
#undef INFINIT_FUSE_FORMALS
#undef INFINIT_FUSE_EFFECTIVE

    ///
    /// this method represents the entry point of the FUSE-specific thread.
    ///
    /// this method is responsible for starting FUSE.
    ///
    void*               FUker::Setup(void*)
    {
      //
      // build the arguments.
      //
      // note that the -h option can be passed in order to display all
      // the available options including the threaded, debug, file system
      // name, file system type etc.
      //
      // for example the -d option could be used instead of -f in order
      // to activate the debug mode.
      //
      elle::String      ofsname("-ofsname=" + Infinit::Network);
      const char*       arguments[] =
        {
          "horizon",

          // XXX
          "-s",

          //
          // this option does not register FUSE as a daemon but
          // run it in foreground.
          //
          "-f",

          //
          // this option indicates the name of the file system type.
          //
          "-osubtype=infinit",

          //
          // this option disables remote file locking.
          //
          "-ono_remote_lock",

          //
          // this option indicates the kernel to perform reads
          // through large chunks.
          //
          "-olarge_read",

          //
          // this option indicates the kernel to perform writes
          // through big writes.
          //
          "-obig_writes",

          //
          // this option activates the in-kernel caching based on
          // the modification times.
          //
          "-oauto_cache",

          //
          // this option indicates the kernel to always forward the I/Os
          // to the filesystem.
          //
          "-odirect_io",

          //
          // this option specifies the name of the file system instance.
          //
          ofsname.c_str(),

          //
          // and finally, the mountpoint.
          //
          Infinit::Mountpoint.c_str()
        };

      struct ::fuse_operations          operations;
      {
        // set all the pointers to zero.
        ::memset(&operations, 0x0, sizeof (::fuse_operations));

        // Fill fuse functions array.
#define INFINIT_FUSE_LINK(Name) operations.Name = BOOST_PP_CAT(Name, _mt)
#define INFINIT_FUSE_LINK_(R, Data, Elem)                       \
        INFINIT_FUSE_LINK(BOOST_PP_TUPLE_ELEM(2, 0, Elem));
        BOOST_PP_SEQ_FOR_EACH(INFINIT_FUSE_LINK_, _, INFINIT_FUSE_COMMANDS)
#undef INFINIT_FUSE_LINK_
#undef INFINIT_FUSE_LINK

        // the following flag being activated prevents the path argument
        // to be passed for functions which take a file descriptor.
        operations.flag_nullpath_ok = 1;
      }

      char* mountpoint;
      int multithreaded;

      if ((FUker::FUSE = ::fuse_setup(
             sizeof (arguments) / sizeof (elle::Character*),
             const_cast<char**>(arguments),
             &operations,
             sizeof (operations),
             &mountpoint,
             &multithreaded,
             nullptr)) == nullptr)
        goto _leave;

      if (multithreaded)
        {
          if (::fuse_loop_mt(FUker::FUSE) == -1)
            goto _leave;
        }
      else
        {
          if (::fuse_loop(FUker::FUSE) == -1)
            goto _leave;
        }

      ::fuse_teardown(FUker::FUSE, mountpoint);

      // reset the FUSE structure pointer.
      FUker::FUSE = nullptr;

      goto _leave;

    _leave:
      // now that FUSE has stopped, make sure the program is exiting.
      new reactor::Thread(elle::concurrency::scheduler(), "exit",
                          &elle::concurrency::Program::Exit, true);
      return nullptr;
    }

    void
    FUker::run()
    {
      // XXX[race conditions exist here:
      //     1) the FUSE thread calls Program::Exit() before our event loop
      //        is entered.
      //     2) using the FUker::FUSE pointer to know if FUSE has been cleaned
      //        is a bad idea since teardown() could have been called, still
      //        the pointer would not be nullptr. there does not seem to be much
      //        to do since we do not control FUSE internal loop and logic.]
      if (::pthread_create(&FUker::Thread,
                           nullptr,
                           &FUker::Setup,
                           nullptr) != 0)
        throw elle::Exception("unable to create the FUSE-specific thread");
    }

    ///
    /// this method initializes the FUker by allocating a broker
    /// for handling the posted events along with creating a specific
    /// thread for FUSE.
    ///
    elle::Status        FUker::Initialize()
    {
      switch (horizon::hole().state())
        {
        case hole::Hole::State::offline:
          {
            hole().ready_hook(&FUker::run);
            break;
          }
          case hole::Hole::State::online:
          {
            FUker::run();
            break;
          }
        }

      return elle::Status::Ok;
    }

    ///
    /// this method cleans the FUker by making sure FUSE exits.
    ///
    elle::Status        FUker::Clean()
    {
      if (FUker::FUSE != nullptr)
        {
          // exit FUSE.
          ::fuse_exit(FUker::FUSE);

          // manually perform a file system call so as to wake up the FUSE
          // worker which is currently blocked waiting for data on the FUSE
          // socket. note that the call will not be treated as FUSE will
          // realise it has exited first.
          //
          // this is quite an un-pretty hack, but nothing has has been found
          // since the FUSE thread is blocked on an I/O.
          struct ::statfs stfs;
          ::statfs(Infinit::Mountpoint.c_str(), &stfs);

          // finally, wait for the FUSE-specific thread to exit.
          if (::pthread_join(FUker::Thread, nullptr) != 0)
            {
              ELLE_WARN("%s", ::strerror(errno));
            }
        }

      return elle::Status::Ok;
    }
  }
}
