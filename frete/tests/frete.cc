#define BOOST_TEST_MODULE Frete
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <fstream>

#include <reactor/Barrier.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/scheduler.hh>

#include <protocol/Serializer.hh>
#include <protocol/ChanneledStream.hh>

#include <frete/Frete.hh>

BOOST_AUTO_TEST_CASE(connection)
{
  reactor::Scheduler sched;

  int port = 0;

  reactor::Barrier listening;

  // Create dummy test fs.
  boost::filesystem::path root("frete/tests/fs");
  boost::filesystem::create_directory(root);

  boost::filesystem::path empty(root / "empty");
  std::ofstream(empty.native());

  boost::filesystem::path content(root / "content");
  {
    std::ofstream f(content.native());
    f << "content\n";
  }

  boost::filesystem::path dir(root / "dir");
  boost::filesystem::create_directory(dir);
  {
    std::ofstream f((dir / "1").native());
    f << "1";
  }
  boost::filesystem::create_directory(dir);
  {
    std::ofstream f((dir / "2").native());
    f << "2";
  }

  reactor::Thread server(
    sched, "server",
    [&]
    {
      reactor::network::TCPServer server(sched);
      server.listen();
      port = server.port();
      listening.open();
      std::unique_ptr<reactor::network::TCPSocket> socket(server.accept());
      infinit::protocol::Serializer serializer(sched, *socket);
      infinit::protocol::ChanneledStream channels(sched, serializer);
      frete::Frete frete(channels);
      frete.add(empty);
      frete.add(content);
      frete.add(dir);
      while (true)
        reactor::sleep(10_sec);
    });

  reactor::Thread client(
    sched, "client",
    [&]
    {
      listening.wait();
      reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
      infinit::protocol::Serializer serializer(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, serializer);
      frete::Frete frete(channels);
      BOOST_CHECK_EQUAL(frete.size(), 4);
      {
        BOOST_CHECK_EQUAL(frete.path(0), "empty");
        for (int i = 0; i < 3; ++i)
        {
          auto buffer = frete.read(0, 0, 1024);
          BOOST_CHECK_EQUAL(buffer.size(), 0);
        }
        {
          auto buffer = frete.read(0, 1, 1024);
          BOOST_CHECK_EQUAL(buffer.size(), 0);
        }
      }
      {
        BOOST_CHECK_EQUAL(frete.path(1), "content");
        {
          auto buffer = frete.read(1, 0, 1024);
          BOOST_CHECK_EQUAL(buffer.size(), 8);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("content\n"));
        }
        {
          auto buffer = frete.read(1, 2, 2);
          BOOST_CHECK_EQUAL(buffer.size(), 2);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("nt"));
        }
        {
          auto buffer = frete.read(1, 7, 2);
          BOOST_CHECK_EQUAL(buffer.size(), 1);
          BOOST_CHECK_EQUAL(buffer, elle::ConstWeakBuffer("\n"));
        }
      }
      {
        BOOST_CHECK_EQUAL(frete.path(2), "dir/2");
        BOOST_CHECK_EQUAL(frete.read(2, 0, 2), elle::ConstWeakBuffer("2"));
        BOOST_CHECK_EQUAL(frete.path(3), "dir/1");
        BOOST_CHECK_EQUAL(frete.read(3, 0, 2), elle::ConstWeakBuffer("1"));
      }
      BOOST_CHECK_THROW(frete.path(4), std::runtime_error);
      BOOST_CHECK_THROW(frete.read(4, 0, 1), std::runtime_error);
      server.terminate();
    });

  sched.run();
}
