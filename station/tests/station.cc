#define BOOST_TEST_MODULE Station
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <cryptography/KeyPair.hh>

#include <reactor/Scope.hh>
#include <reactor/network/buffer.hh>
#include <reactor/scheduler.hh>

#include <station/AlreadyConnected.hh>
#include <station/Host.hh>
#include <station/InvalidPassport.hh>
#include <station/Station.hh>

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

BOOST_AUTO_TEST_CASE(construction)
{
  reactor::Scheduler sched;

  reactor::Thread t(sched, "main", []
                    {
                      Credentials c("host");
                      station::Station station(authority, c.passport);
                    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(connection)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      // FIXME: ipv4
      auto host1 = station1.connect("127.0.0.1", station2.port());
      BOOST_CHECK(!station1.host_available());
      auto host2 = station2.accept();
      BOOST_CHECK(!station2.host_available());
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(connection_fails)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      // FIXME: reactor should have specific network exceptions.
      BOOST_CHECK_THROW(station1.connect("127.0.0.1", 4242),
                        std::runtime_error);
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(connection_closed)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
    {
      reactor::network::TCPServer s(*reactor::Scheduler::scheduler());
      s.listen();
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      reactor::Scope scope;
      scope.run_background("connect", [&]
                           {
                             BOOST_CHECK_THROW(station1.connect("127.0.0.1", s.port()),
                                               std::runtime_error);
                           });
      scope.run_background("ignore", [&]
                           {
                             delete s.accept();
                           });
      scope.wait();
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(already_connected)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      auto host = station1.connect("127.0.0.1", station2.port());
      BOOST_CHECK_THROW(
        station1.connect("127.0.0.1", station2.port()),
        station::AlreadyConnected);
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(reconnect)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
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
  sched.run();
}

BOOST_AUTO_TEST_CASE(destruct_pending)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      auto host = station1.connect("127.0.0.1", station2.port());
      // Destruct station2 while it has a host pending.
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(double_connection)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", []
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      Credentials c2("host2");
      station::Station station2(authority, c2.passport);
      // FIXME: ipv4
      reactor::Scope scope;
      std::unique_ptr<station::Host> host1;
      std::unique_ptr<station::Host> host2;
      int already = 0;
      scope.run_background(
        "1 -> 2",
        [&]
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
      scope.run_background(
        "2 -> 1",
        [&]
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
      host1->socket().write("one");
      host2->socket().read(reactor::network::Buffer(buf, 3));
      BOOST_CHECK_EQUAL(buf, "one");
      host2->socket().write("two");
      host1->socket().read(reactor::network::Buffer(buf, 3));
      BOOST_CHECK_EQUAL(buf, "two");
    });
  sched.run();
}

BOOST_AUTO_TEST_CASE(connection_close)
{
  reactor::Scheduler sched;

  reactor::Thread t(
    sched, "main", [&]
    {
      Credentials c1("host1");
      station::Station station1(authority, c1.passport);
      {
        reactor::network::TCPSocket socket(sched, "127.0.0.1", station1.port());
      }
      reactor::sleep(1_sec);
    });
  sched.run();
}
