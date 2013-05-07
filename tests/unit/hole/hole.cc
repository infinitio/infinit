#define BOOST_TEST_MODULE Hole
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <cryptography/KeyPair.hh>

#include <elle/serialize/extract.hh>

#include <common/common.hh>

#include <hole/Hole.hh>
#include <hole/implementations/slug/Slug.hh>
#include <hole/storage/Memory.hh>
#include <hole/Passport.hh>
#include <nucleus/proton/Network.hh>
#include <Infinit.hh>
#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

void
Main()
{
  nucleus::proton::Network n("test network");
  hole::storage::Memory mem(n);
  infinit::cryptography::KeyPair keys =
    infinit::cryptography::KeyPair::generate(
      infinit::cryptography::Cryptosystem::rsa, 1024);
  infinit::cryptography::KeyPair authority_keys =
    infinit::cryptography::KeyPair::generate(
      infinit::cryptography::Cryptosystem::rsa, 1024);
  elle::Authority authority(authority_keys);
  elle::Passport passport("0xdeadbeef", "host1", keys.K(), authority);

  std::vector<elle::network::Locus> members;
  std::unique_ptr<hole::Hole> h(
    new hole::implementations::slug::Slug(
      mem, passport, authority,
      reactor::network::Protocol::tcp, members, 0,
      boost::posix_time::milliseconds(5000)));
}

BOOST_AUTO_TEST_CASE(test_hole)
{
  reactor::Scheduler sched;

  reactor::Thread t(sched,
                    "main",
                    [&] {Main();});
  sched.run();
}
