#include <elle/test.hh>

#include <cryptography/KeyPair.hh>

#include <reactor/network/buffer.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

#include <station/AlreadyConnected.hh>
#include <station/Host.hh>
#include <station/InvalidPassport.hh>
#include <station/Station.hh>

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

enum class NegotiationStatus
{
  already_connected,
  invalid,
  succeeded,
};

class StationInstrumentation
{
public:
  StationInstrumentation(station::Station& station)
    : _station(station)
    , _server()
    , _serve_thread("serve",
                    std::bind(&StationInstrumentation::_serve, std::ref(*this)))
    , _passport_barrier()
    , _status_to_barrier()
    , _status_from_barrier()
  {
    this->_server.listen();
  }

  ~StationInstrumentation()
  {
    this->_serve_thread.terminate_now();
  }

  void
  _serve()
  {
    auto a = this->_server.accept();
    reactor::network::TCPSocket b("127.0.0.1", this->_station.port());
    auto forward = [this] (reactor::network::Socket& a,
                           reactor::network::Socket& b,
                           reactor::Barrier& status_barrier)
      {
        auto protocol = a.read(1);
        b.write(protocol);
        b.flush();
        reactor::wait(this->_passport_barrier);
        elle::serialize::InputBinaryArchive input(a);
        elle::serialize::OutputBinaryArchive output(b);
        papier::Passport passport;
        input >> passport;
        output << passport;
        b.flush();
        reactor::wait(status_barrier);
        NegotiationStatus status;
        input >> status;
        output << status;
        b.flush();
        reactor::sleep();
      };
    elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
    {
      scope.run_background("a -> b",
                           std::bind(forward, std::ref(*a), std::ref(b),
                                     std::ref(this->_status_to_barrier)));
      scope.run_background("b -> a",
                           std::bind(forward, std::ref(b), std::ref(*a),
                                     std::ref(this->_status_from_barrier)));
      reactor::wait(scope);
    };
  }

  int
  port() const
  {
    return this->_server.port();
  }

  ELLE_ATTRIBUTE_R(station::Station&, station);
  ELLE_ATTRIBUTE_R(reactor::network::TCPServer, server);
  ELLE_ATTRIBUTE(reactor::Thread, serve_thread);
  ELLE_ATTRIBUTE_RX(reactor::Barrier, passport_barrier);
  ELLE_ATTRIBUTE_RX(reactor::Barrier, status_to_barrier);
  ELLE_ATTRIBUTE_RX(reactor::Barrier, status_from_barrier);
};

ELLE_TEST_SCHEDULED(connect_close_connect, (bool, swap))
{
  Credentials c1("host1");
  Credentials c2("host2");
  auto& master = c1.passport < c2.passport ? c1.passport : c2.passport;
  auto& slave = c1.passport < c2.passport ? c2.passport : c1.passport;
  station::Station station1(authority, std::min(c1.passport, c2.passport));
  station::Station station2(authority, std::max(c1.passport, c2.passport));
  StationInstrumentation instrumentation1(station1);
  StationInstrumentation instrumentation2(station2);
  reactor::Barrier sync;
  std::unique_ptr<station::Host> host;
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    scope.run_background("1 -> 2", [&]
    {
      try
      {
        instrumentation2.passport_barrier().open();
        instrumentation2.status_from_barrier().open();
        station1.connect("127.0.0.1", instrumentation2.port());
        instrumentation1.passport_barrier().open();
        instrumentation1.status_from_barrier().open();
        if (swap)
          instrumentation2.status_to_barrier().open();
        else
          instrumentation1.status_to_barrier().open();
        reactor::wait(sync);
        if (swap)
          instrumentation1.status_to_barrier().open();
        else
          instrumentation2.status_to_barrier().open();
        reactor::sleep(1_sec);
      }
      catch (station::AlreadyConnected const&)
      {}
    });
    scope.run_background("2 -> 1", [&]
    {
      bool caught = false;
      try
      {
        host = station2.connect("127.0.0.1", instrumentation1.port());
        sync.open();
      }
      catch (station::AlreadyConnected const&)
      {}
      catch (station::ConnectionFailure const&)
      {
        // FIXME: conflict will be fixed after 0.9.7.
        sync.open();
        caught = true;
      }
      if (swap)
        BOOST_CHECK(caught);
    });
    scope.wait();
  };
}

ELLE_TEST_SUITE()
{
  auto timeout = valgrind(20);
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
  auto connect_close_connect_first = std::bind(&connect_close_connect, true);
  suite.add(BOOST_TEST_CASE(connect_close_connect_first), 0, timeout);
  auto connect_close_connect_second = std::bind(&connect_close_connect, false);
  suite.add(BOOST_TEST_CASE(connect_close_connect_second), 0, timeout);
}
