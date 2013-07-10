#define BOOST_TEST_MODULE Hole
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <cryptography/KeyPair.hh>

#include <elle/serialize/extract.hh>

#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>
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

static
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

static
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

static
void
two_slugs_push_pull_async()
{
  nucleus::proton::Network n("test network");
  Slug slug1("1", n);

  nucleus::neutron::Group g(n, slug1.keys.K(), "towel");
  g.seal(slug1.keys.k());
  auto addr = g.bind();
  slug1.slug.push(addr, g);

  std::vector<elle::network::Locus> members;
  members.push_back(elle::network::Locus("127.0.0.1", slug1.slug.port()));
  Slug slug2("2", n, members);

  auto pulled = slug2.slug.pull(addr, nucleus::proton::Revision::Last);
  auto pulled_group = dynamic_cast<nucleus::neutron::Group*>(pulled.get());
  BOOST_CHECK(pulled_group);
  BOOST_CHECK_EQUAL(pulled_group->description(), "towel");
}

BOOST_AUTO_TEST_CASE(test_two_slugs_push_pull_async)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched,
                    "main",
                    &two_slugs_push_pull_async);
  sched.run();
}

static
void
two_slugs_nasty_connect()
{
  nucleus::proton::Network n("test network");
  Slug slug1("slug1", n);
  Slug slug2("slug2", n);

  slug1.slug.portal_connect("127.0.0.1", slug2.slug.port(), false);
  BOOST_CHECK_EQUAL(slug1.slug.hosts().size(), 1);
  BOOST_CHECK_EQUAL(slug2.slug.hosts().size(), 1);
  BOOST_CHECK_THROW(
    slug2.slug.portal_connect("127.0.0.1", slug1.slug.port(), false),
    elle::Exception
    );
  BOOST_CHECK_EQUAL(slug1.slug.hosts().size(), 1);
  BOOST_CHECK_EQUAL(slug2.slug.hosts().size(), 1);
}

BOOST_AUTO_TEST_CASE(test_two_slugs_nasty_connect)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched,
                    "main",
                    &two_slugs_nasty_connect);
  sched.run();
}

static
void
two_slugs_nasty_connect_parallel()
{
  nucleus::proton::Network n("test network");
  Slug slug1("slug1", n);
  Slug slug2("slug2", n);

  int errors = 0;
  {
    reactor::Scope scope;
    scope.run_background("connect1", [&errors, &slug1, &slug2] {
        try
        {
          slug1.slug.portal_connect("127.0.0.1", slug2.slug.port(), false);
        }
        catch (...)
        {
          ++errors;
        }
      });
    scope.run_background("connect2", [&errors, &slug1, &slug2] {
        try
        {
          slug2.slug.portal_connect("127.0.0.1", slug1.slug.port(), false);
        }
        catch (...)
        {
          ++errors;
        }
      });
    // XXX: join scope
    reactor::Scheduler::scheduler()->current()->sleep(1_sec);
  }

  BOOST_CHECK_EQUAL(slug1.slug.hosts().size(), 1);
  BOOST_CHECK_EQUAL(slug2.slug.hosts().size(), 1);
  BOOST_CHECK_EQUAL(errors, 1);
}

BOOST_AUTO_TEST_CASE(test_two_slugs_nasty_connect_parallel)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched,
                    "main",
                    &two_slugs_nasty_connect_parallel);
  sched.run();
}
