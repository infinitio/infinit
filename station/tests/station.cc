#include <elle/test.hh>

#include <cryptography/KeyPair.hh>

#include <reactor/network/buffer.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

#include <station/AlreadyConnected.hh>
#include <station/Host.hh>
#include <station/InvalidPassport.hh>
#include <station/Station.hh>

#ifdef VALGRIND
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND 0
#endif

ELLE_LOG_COMPONENT("station.test")

infinit::cryptography::KeyPair authority_keys =
  infinit::cryptography::KeyPair::generate(
  infinit::cryptography::Cryptosystem::rsa, 1024);
papier::Authority authority(authority_keys);

// Helper to quickly generate a keypair and related passport.
struct Credentials
{
  Credentials(std::string const& name):
    keys(infinit::cryptography::KeyPair::generate(
           infinit::cryptography::Cryptosystem::rsa, 1024)),
    passport(elle::sprintf("passport_%s", name),
             elle::sprintf("host_%s", name),
             keys.K(),
             authority)
  {}

  infinit::cryptography::KeyPair keys;
  papier::Passport passport;
};

ELLE_TEST_SCHEDULED(construction)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c("host");
      station::Station station(authority, c.passport);
    });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(connection)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      // FIXME: ipv4.
      auto host1 = station1.connect("127.0.0.1", station2.port());
      BOOST_CHECK(!station1.host_available());
      auto host2 = station2.accept();
      BOOST_CHECK(!station2.host_available());
    });
    scope.wait();
  };
}

// Connection refused entails infinite wait on wine.
#ifndef INFINIT_WINDOWS
ELLE_TEST_SCHEDULED(connection_fails)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      // FIXME: reactor should have specific network exceptions.
      BOOST_CHECK_THROW(station1.connect("127.0.0.1", 4242),
                        std::runtime_error);
    });
    scope.wait();
  };
}
#endif

ELLE_TEST_SCHEDULED(connection_closed)
{
  auto s = elle::make_unique<reactor::network::TCPServer>();
  s->listen();
  ELLE_LOG("server listens on %s", s->port());
  Credentials c1("host1");
  station::Station station1(authority, c1.passport);

  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("connect", [&]
    {
      ELLE_LOG_SCOPE("connect station");
      BOOST_CHECK_THROW(station1.connect("127.0.0.1", s->port()),
                        std::runtime_error);
    });
    scope.run_background("ignore", [&]
    {
      s->accept();
      ELLE_LOG_SCOPE("close socket server side");
    });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(already_connected)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      auto host = station1.connect("127.0.0.1", station2.port());
      BOOST_CHECK_THROW(station1.connect("127.0.0.1", station2.port()),
                        station::AlreadyConnected);
    });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(reconnect)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      auto host1 = station1.connect("127.0.0.1", station2.port());
      auto host2 = station2.accept();
      host1.reset(); // Disconnect
      host2.reset(); // Disconnect
      host1 = station1.connect("127.0.0.1", station2.port());
      host2 = station2.accept();
    });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(destruct_pending)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      auto host = station1.connect("127.0.0.1", station2.port());
      // Destruct station2 while it has a host pending.
    });
    scope.wait();
  };
}

ELLE_TEST_SCHEDULED(double_connection)
{
  Credentials c1("host1");
  station::Station station1(authority, c1.passport);
  Credentials c2("host2");
  station::Station station2(authority, c2.passport);
  // FIXME: ipv4
  std::unique_ptr<station::Host> host1;
  std::unique_ptr<station::Host> host2;
  int already = 0;

  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("1 -> 2", [&]
    {
      try
      {
        host1 = station1.connect("127.0.0.1", station2.port());
      }
      catch (station::AlreadyConnected const&)
      {
        ++already;
      }
    });
    scope.run_background("2 -> 1", [&]
    {
      try
      {
        host2 = station2.connect("127.0.0.1", station1.port());
      }
      catch (station::AlreadyConnected const&)
      {
        ++already;
      }
    });
    scope.wait();
    BOOST_CHECK_EQUAL(already, 1);
    if (host1)
      host2 = station2.accept();
    else
      host1 = station1.accept();
    BOOST_CHECK(!station1.host_available());
    BOOST_CHECK(!station2.host_available());
    char buf[4];
    buf[3] = 0;
    host1->socket().write(elle::ConstWeakBuffer("one"));
    host2->socket().read(reactor::network::Buffer(buf, 3));
    BOOST_CHECK_EQUAL(buf, "one");
    host2->socket().write(elle::ConstWeakBuffer("two"));
    host1->socket().read(reactor::network::Buffer(buf, 3));
    BOOST_CHECK_EQUAL(buf, "two");
  };
}

ELLE_TEST_SCHEDULED(connection_close)
{
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      {
        reactor::network::TCPSocket socket("127.0.0.1", station1.port());
      }
      reactor::sleep(1_sec);
    });
    scope.wait();
  };
}

ELLE_TEST_SUITE()
{
  auto timeout = RUNNING_ON_VALGRIND ? 15 : 3;
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(construction), 0, timeout);
  suite.add(BOOST_TEST_CASE(connection), 0, timeout);
#ifndef INFINIT_WINDOWS
  suite.add(BOOST_TEST_CASE(connection_fails), 0, timeout);
#endif
  suite.add(BOOST_TEST_CASE(connection_closed), 0, timeout);
  suite.add(BOOST_TEST_CASE(already_connected), 0, timeout);
  suite.add(BOOST_TEST_CASE(reconnect), 0, timeout);
  suite.add(BOOST_TEST_CASE(destruct_pending), 0, timeout);
  suite.add(BOOST_TEST_CASE(double_connection), 0, timeout);
  suite.add(BOOST_TEST_CASE(connection_close), 0, timeout);
}
