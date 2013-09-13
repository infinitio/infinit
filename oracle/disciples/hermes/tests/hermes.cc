#define BOOST_TEST_MODULE Hermes

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <reactor/scheduler.hh>
#include <elle/Exception.hh>

#include <oracle/disciples/hermes/src/hermes/Hermes.hh>

// TODO: Try to do a better job at comparing both buffers.

static const std::string base_path = std::string("/tmp/hermes");
static const char* host = "127.0.0.1";
static const int port = 4242;

static
void
server()
{
  auto& sched = *reactor::Scheduler::scheduler();
  oracle::hermes::Hermes herm(sched, port, base_path);

  herm.run();
};

BOOST_AUTO_TEST_CASE(test_identification)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid1("transaction1");
  oracle::hermes::TID tid2("transaction2");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    // Check simple case with identification first.
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);
      oracle::hermes::HermesRPC handler(channels);

      handler.ident(tid1);

      std::string msg("This is basic content");
      elle::Buffer input(msg.c_str(), msg.size());
      BOOST_CHECK_EQUAL(handler.store(1, 1, input), input.size());
      BOOST_CHECK_EQUAL(handler.fetch(1, 1).size(), input.size());
    }

    // Try to retrieve previously stored info with different ident number.
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);
      oracle::hermes::HermesRPC handler(channels);

      handler.ident(tid2);

      // Should throw because tid2 does not have a 1, 1 chunk.
      BOOST_CHECK_THROW(handler.fetch(1, 1), elle::Exception);

      // When storing a 1, 1 chunk in tid2 we can retrieve it fine.
      std::string msg("This is some other basic content");
      elle::Buffer input(msg.c_str(), msg.size());
      BOOST_CHECK_EQUAL(handler.store(1, 1, input), input.size());
      BOOST_CHECK_EQUAL(handler.fetch(1, 1).size(), input.size());
    }

    // Check simple case without identifying first.
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);
      oracle::hermes::HermesRPC handler(channels);

      // Try to store and fetch before identifying.
      std::string msg("This is yet another basic content");
      elle::Buffer input(msg.c_str(), msg.size());
      BOOST_CHECK_THROW(handler.store(1, 2, input), elle::Exception);
      BOOST_CHECK_THROW(handler.fetch(1, 2), elle::Exception);
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "dispatcher", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(test_multiple_chunks)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction10");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, host, port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    // Identify first
    BOOST_CHECK_NO_THROW(handler.ident(tid));

    // Store first.
    std::string msg1("This is basic content for first msg");
    elle::Buffer input1(msg1.c_str(), msg1.size());
    BOOST_CHECK_EQUAL(handler.store(1, 1, input1), input1.size());

    // Store second.
    std::string msg2("Little message");
    elle::Buffer input2(msg2.c_str(), msg2.size());
    BOOST_CHECK_EQUAL(handler.store(1, 2, input2), input2.size());

    // Fetch second.
    BOOST_CHECK_EQUAL(handler.fetch(1, 2).size(), input2.size());

    // Store third.
    std::string msg3("This message is designed to be longer that others.");
    elle::Buffer input3(msg3.c_str(), msg3.size());
    BOOST_CHECK_EQUAL(handler.store(1, 3, input3), input3.size());

    // Fetch first.
    BOOST_CHECK_EQUAL(handler.fetch(1, 1).size(), input1.size());

    // Fetch second.
    BOOST_CHECK_EQUAL(handler.fetch(1, 2).size(), input2.size());

    // Fetch third.
    BOOST_CHECK_EQUAL(handler.fetch(1, 3).size(), input3.size());

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "dispatcher", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

#if 0
BOOST_AUTO_TEST_CASE(test_filenotfound)
{
  reactor::Scheduler sched;
  int port(4242);

  auto server = [=] ()
  {
    auto& sched = *reactor::Scheduler::scheduler();
    oracle::hermes::Hermes herm(sched, port, base_path);

    herm.run();
  };

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    BOOST_CHECK_THROW(handler.fetch(3, 1), elle::Exception);

    // Store first file.
    std::string msg1("This is a common file content. ab cd-ef_gh");
    elle::Buffer input1(msg1.c_str(), msg1.size());
    BOOST_CHECK_EQUAL(handler.store(3, 1, input1), input1.size());

    // Try to fetch inexisting file.
    BOOST_CHECK_THROW(handler.fetch(3, 2), elle::Exception);

    // Store second file.
    std::string msg2("This is another common file chunk or whatever");
    elle::Buffer input2(msg2.c_str(), msg2.size());
    BOOST_CHECK_EQUAL(handler.store(3, 2, input2), input2.size());

    // Fetch existing files.
    BOOST_CHECK_NO_THROW(handler.fetch(3, 1));
    BOOST_CHECK_NO_THROW(handler.fetch(3, 2));
    BOOST_CHECK_EQUAL(handler.fetch(3, 1).size(), input1.size());
    BOOST_CHECK_EQUAL(handler.fetch(3, 2).size(), input2.size());

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "dispatcher", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
  clean(base_path);
}

BOOST_AUTO_TEST_CASE(test_file_already_present)
{
  reactor::Scheduler sched;
  int port(4242);

  auto server = [=] ()
  {
    auto& sched = *reactor::Scheduler::scheduler();
    oracle::hermes::Hermes herm(sched, port, base_path);

    herm.run();
  };

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    // Store and fetch first file.
    std::string msg1("This is a random file as well");
    elle::Buffer input1(msg1.c_str(), msg1.size());
    BOOST_CHECK_EQUAL(handler.store(4, 1, input1), input1.size());
    BOOST_CHECK_EQUAL(handler.fetch(4, 1).size(), input1.size());

    // Try and store a file with the same FileID/Offset pair.
    std::string msg2("This file cannot be created");
    elle::Buffer input2(msg2.c_str(), msg2.size());
    BOOST_CHECK_THROW(handler.store(4, 1, input2), elle::Exception);

    // Fetch first file and check it's actually the first file.
    BOOST_CHECK_NO_THROW(handler.fetch(4, 1));
    BOOST_CHECK_EQUAL(handler.fetch(4, 1).size(), input1.size());

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "dispatcher", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
  clean(base_path);
}

BOOST_AUTO_TEST_CASE(test_bad_basepath)
{
  reactor::Scheduler sched;
  int port(4242);

  auto server = [=] ()
  {
    auto& sched = *reactor::Scheduler::scheduler();
    BOOST_CHECK_THROW(oracle::hermes::Hermes herm(sched, port, "/dev/null"),
                      elle::Exception);
  };

  reactor::Thread serv(sched, "dispatcher", server);

  sched.run();
  clean(base_path);
}

BOOST_AUTO_TEST_CASE(test_restart)
{
  reactor::Scheduler sched;
  int port(4242);

  auto first_server = [=] ()
  {
    auto& sched = *reactor::Scheduler::scheduler();
    oracle::hermes::Hermes herm(sched, port, base_path);

    herm.run();
  };

  auto second_server = [=] (reactor::Thread* to_wait)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    sched.current()->wait(*to_wait);

    oracle::hermes::Hermes herm(sched, port, base_path);

    herm.run();
  };

  auto first_client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    std::string msg("Hello there, I am a bit of file.");
    elle::Buffer input(msg.c_str(), msg.size());
    BOOST_CHECK_EQUAL(handler.store(5, 1, input), input.size());
    BOOST_CHECK_EQUAL(handler.fetch(5, 1).size(), input.size());

    serv->terminate_now();
  };

  auto second_client = [=] (reactor::Thread* to_wait, reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    sched.current()->wait(*to_wait);

    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    BOOST_CHECK_NO_THROW(handler.fetch(5, 1));
    // 28 is the size of the previously stored file.
    BOOST_CHECK_EQUAL(handler.fetch(5, 1).size(), 32);

    serv->terminate_now();
  };

  reactor::Thread serv1(sched, "dispatcher1", first_server);
  reactor::Thread cli1(sched, "client1", std::bind(first_client, &serv1));

  reactor::Thread serv2(sched, "dispatcher2", std::bind(second_server, &cli1));
  reactor::Thread cli2(sched, "client2", std::bind(second_client, &cli1, &serv2));

  sched.run();
  clean(base_path);
}

BOOST_AUTO_TEST_CASE(test_restart_fail)
{
  reactor::Scheduler sched;
  int port(4242);

  auto first_server = [=] ()
  {
    auto& sched = *reactor::Scheduler::scheduler();
    oracle::hermes::Hermes herm(sched, port, base_path);

    herm.run();
  };

  auto second_server = [=] (reactor::Thread* to_wait)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    sched.current()->wait(*to_wait);

    oracle::hermes::Hermes herm(sched, port, base_path);

    herm.run();
  };

  auto first_client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    std::string msg("Hello there, I am a bit of file.");
    elle::Buffer input(msg.c_str(), msg.size());
    BOOST_CHECK_EQUAL(handler.store(6, 1, input), input.size());
    BOOST_CHECK_EQUAL(handler.fetch(6, 1).size(), input.size());

    serv->terminate_now();
  };

  auto second_client = [=] (reactor::Thread* to_wait, reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    sched.current()->wait(*to_wait);

    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    BOOST_CHECK_THROW(handler.fetch(99, 1), elle::Exception);

    serv->terminate_now();
  };

  reactor::Thread serv1(sched, "dispatcher1", first_server);
  reactor::Thread cli1(sched, "client1", std::bind(first_client, &serv1));

  reactor::Thread serv2(sched, "dispatcher2", std::bind(second_server, &cli1));
  reactor::Thread cli2(sched, "client2", std::bind(second_client, &cli1, &serv2));

  sched.run();
  clean(base_path);
}

BOOST_AUTO_TEST_CASE(test_parallel)
{
  reactor::Scheduler sched;
  int port(4242);

  auto server = [&] ()
  {
    oracle::hermes::Hermes(sched, port, base_path).run();
  };

  auto client1 = [&] (reactor::Thread* serv)
  {
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    std::string msg("Hello there, I am a bit of another file.");
    elle::Buffer input(msg.c_str(), msg.size());
    BOOST_CHECK_EQUAL(handler.store(7, 1, input), input.size());

    sched.current()->wait(*serv);
  };

  auto client2 = [&] (reactor::Thread* serv)
  {
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);

    std::string msg("Hello there, I am a bit of yet another file.");
    elle::Buffer input(msg.c_str(), msg.size());
    BOOST_CHECK_EQUAL(handler.store(7, 2, input), input.size());

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "serv", server);
  reactor::Thread cli1(sched, "client1", std::bind(client1, &serv));
  reactor::Thread cli2(sched, "client2", std::bind(client2, &serv));

  sched.run();
  clean(base_path);
}
#endif
