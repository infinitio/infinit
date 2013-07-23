#include <surface/gap/State.hh>

#include <lune/Lune.hh>
#include <hole/Passport.hh>
#include <plasma/meta/Client.hh>
#include <surface/gap/_detail/TransferOperations.hh>

#include <reactor/fsm.hh>
#include <reactor/network/Protocol.hh>
#include <reactor/scheduler.hh>
#include <reactor/thread.hh>
#include <reactor/waitable.hh>

#include <reactor/duration.hh>
#include <reactor/sleep.hh>

#include <elle/os/file.hh>
#include <elle/log.hh>

#include <boost/filesystem.hpp>

#include <functional>
#include <string>
#include <unordered_set>
#include <memory>

ELLE_LOG_COMPONENT("a");

inline
reactor::Scheduler&
sched()
{
  static reactor::Scheduler scheduler;
  return scheduler;
}

int
main(int argc, char** argv)
{
  lune::Lune::Initialize();

  static elle::Status e = elle::Status::Error;

  if (argc == 1)
  {
    surface::gap::State sender;

    try
    {
      sender.register_("dam", "damrok@infinit.io", std::string(64, 'c'), "bitebite");
    }
    catch (...)
    {
      sender.login("damrok@infinit.io", std::string(64, 'c'));
    }

    sender.send_files("courgemolle@infinit.io",
                      {"/home/dimrok/Downloads/control.tar.gz"});

    reactor::Thread trigger_accepted(sched(), "accepted", [&] () {
        reactor::Sleep s(sched(), 100_sec);
        s.run();
      });

    sched().run();

  }

  //   elle::Passport passport{};
  //   elle::serialize::from_file(common::infinit::passport_path(sender.me().id)) >> passport;
  //   lune::Identity identity{};

  //   if (identity.Restore(sender.meta().identity()) == e)
  //     throw elle::Exception("Couldn't restore the identity.");

  //   surface::gap::SendingMachine sm(sender.meta(),
  //                                   sender.me().id,
  //                                   sender.device_id(),
  //                                   passport,
  //                                   identity,
  //                                   "courgemolle@infinit.io",
  //                                   {"/home/dimrok/Downloads/control.tar.gz"});

  //   sched().run();
  // }
  // else
  // {
  //   surface::gap::State recipient;

  //   try
  //   {
  //     recipient.register_("dum", "courgemolle@infinit.io", std::string(64, 'c'), "bitebite");
  //   }
  //   catch (...)
  //   {
  //     recipient.login("courgemolle@infinit.io", std::string(64, 'c'));
  //   }

  //   elle::Passport passport{};
  //   recipient.device();
  //   elle::serialize::from_file(common::infinit::passport_path(recipient.me().id)) >> passport;
  //   lune::Identity identity{};

  //   if (identity.Restore(recipient.meta().identity()) == e)
  //     throw elle::Exception("Couldn't restore the identity.");

  //   auto transaction_id = std::string(argv[1]);
  //   std::cerr << transaction_id << std::endl;

  //   surface::gap::ReceivingMachine rm(
  //     recipient.meta(), recipient.me().id, recipient.device_id(), passport,
  //     identity, transaction_id);

  //   reactor::Thread trigger_accepted(sched(), "accepted", [&] () {
  //       reactor::Sleep s(sched(), 5_sec);
  //       s.run();
  //       rm.accept();
  //     });

  //   sched().run();
  // }
}
