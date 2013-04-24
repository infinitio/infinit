#define BOOST_TEST_MODULE Hole
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <hole/Hole.hh>
#include <hole/implementations/slug/Implementation.hh>
#include <hole/storage/Memory.hh>
#include <hole/Passport.hh>
#include <nucleus/proton/Network.hh>
#include <common/common.hh>
#include <elle/serialize/extract.hh>
#include <Infinit.hh>
#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

void
Main()
{
  nucleus::proton::Network n{"test network"};
  hole::storage::Memory mem{n};
  elle::Passport passport{
    elle::serialize::from_file(common::infinit::passport_path("5166eafde77989454a000003"))
  };

  std::vector<elle::network::Locus> members;
  std::unique_ptr<hole::Hole> h{
    new hole::implementations::slug::Implementation{
      mem, passport, Infinit::authority(),
      reactor::network::Protocol::tcp, members, 0,
      boost::posix_time::milliseconds(5000)}};
  h->join();
}

BOOST_AUTO_TEST_CASE(hole)
{
  reactor::Scheduler sched;

  reactor::Thread t(sched,
                    "main",
                    [&] {Main();});
  sched.run();
}
