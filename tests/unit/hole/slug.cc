#define BOOST_TEST_MODULE Hole
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <cryptography/KeyPair.hh>

#include <elle/serialize/extract.hh>

#include <reactor/scheduler.hh>
#include <reactor/thread.hh>

#include <common/common.hh>
#include <hole/Hole.hh>
#include <hole/implementations/slug/Slug.hh>
#include <hole/storage/Memory.hh>
#include <hole/Passport.hh>
#include <nucleus/factory.hh>
#include <nucleus/neutron/Component.hh>
#include <nucleus/neutron/Group.hh>
#include <nucleus/proton/ContentHashBlock.hh>
#include <nucleus/proton/Network.hh>
#include <Infinit.hh>

infinit::cryptography::KeyPair authority_keys =
  infinit::cryptography::KeyPair::generate(
  infinit::cryptography::Cryptosystem::rsa, 1024);
elle::Authority authority(authority_keys);


struct Slug
{
public:
  Slug(std::string const& name,
       nucleus::proton::Network const& network,
       std::vector<elle::network::Locus> const& members =
         std::vector<elle::network::Locus>()):
    storage(network),
    keys(infinit::cryptography::KeyPair::generate(
           infinit::cryptography::Cryptosystem::rsa, 1024)),
    passport(elle::sprintf("passport_%s", name),
             elle::sprintf("host_%s", name),
             keys.K(),
             authority),
    slug(storage,
         passport,
         authority,
         reactor::network::Protocol::tcp,
         members,
         0,
         boost::posix_time::milliseconds(5000))
  {}

  hole::storage::Memory storage;
  infinit::cryptography::KeyPair keys;
  elle::Passport passport;
  hole::implementations::slug::Slug slug;
};

void
slug_push_pull()
{
  nucleus::proton::Network n("test network");
  Slug slug("slug", n);

  nucleus::neutron::Group g(n, slug.keys.K(), "towel");
  g.seal(slug.keys.k());
  auto addr = g.bind();
  slug.slug.push(addr, g);

  auto pulled = slug.slug.pull(addr, nucleus::proton::Revision::Last);
  auto pulled_group = dynamic_cast<nucleus::neutron::Group*>(pulled.get());
  BOOST_CHECK(pulled_group);
  BOOST_CHECK_EQUAL(pulled_group->description(), "towel");
}

BOOST_AUTO_TEST_CASE(test_slug_push_pull)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched,
                    "main",
                    &slug_push_pull);
  sched.run();
}

void
two_slugs_push_pull()
{
  nucleus::proton::Network n("test network");
  Slug slug1("1", n);

  std::vector<elle::network::Locus> members;
  members.push_back(elle::network::Locus("127.0.0.1", slug1.slug.port()));
  Slug slug2("2", n, members);

  nucleus::neutron::Group g(n, slug1.keys.K(), "towel");
  g.seal(slug1.keys.k());
  auto addr = g.bind();
  slug1.slug.push(addr, g);

  reactor::Scheduler::scheduler()->current()->sleep(boost::posix_time::milliseconds(100));

  auto pulled = slug2.slug.pull(addr, nucleus::proton::Revision::Last);
  auto pulled_group = dynamic_cast<nucleus::neutron::Group*>(pulled.get());
  BOOST_CHECK(pulled_group);
  BOOST_CHECK_EQUAL(pulled_group->description(), "towel");
}

BOOST_AUTO_TEST_CASE(test_two_slugs_push_pull)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched,
                    "main",
                    &two_slugs_push_pull);
  sched.run();
}
