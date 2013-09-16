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
  oracle::hermes::Hermes(sched, port, base_path).run();
};

static
bool
test_content(std::string msg, std::string tid, uint64_t id, uint64_t off)
{
  std::string path(base_path + "/" + tid + "/");
  path += std::to_string(id) + "_" + std::to_string(off) + ".blk";

  char* output = new char[msg.size() + 1];
  output[msg.size()] = 0;

  std::ifstream in(path.c_str());
  in.read(output, msg.size());
  in.close();

  std::cout << msg << " =? " << std::string(output) << std::endl;

  bool result(std::string(output) == msg);
  delete output;
  return result;
}


BOOST_AUTO_TEST_CASE(test_identification)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid1("transaction1");
  oracle::hermes::TID tid2("transaction2");
  oracle::hermes::TID tid3("transaction3");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    // Test append function.
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid1);

      // First simple message.
      std::string msg1("This is basic content");
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid1, 1, 0));

      // Append another message.
      std::string msg2(" and this is the rest of the content");
      elle::Buffer input2(msg2.c_str(), msg2.size());
      BOOST_CHECK_EQUAL(handler.store(1, msg1.size(), input2), input2.size());
      BOOST_CHECK(test_content(msg1 + msg2, tid1, 1, 0));

      // Append another message.
      std::string msg3(" followed by even more content.");
      elle::Buffer input3(msg3.c_str(), msg3.size());
      BOOST_CHECK_EQUAL(handler.store(1, msg1.size() + msg2.size(), input3), input3.size());
      BOOST_CHECK(test_content(msg1 + msg2 + msg3, tid1, 1, 0));

      // Append another message.
      std::string msg4(" MORE!");
      elle::Buffer input4(msg4.c_str(), msg4.size());
      BOOST_CHECK_EQUAL(handler.store(1, msg1.size() + msg2.size() + msg3.size(), input4), input4.size());
      BOOST_CHECK(test_content(msg1 + msg2 + msg3 + msg4, tid1, 1, 0));
    }

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid2);

      // First simple message.
      std::string msg1("This is basic content");
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid2, 1, 0));

      // Second message that oerlaps with first one.
      std::string msg2("content that overlaps");
      elle::Buffer input2(msg2.c_str(), msg2.size());
      BOOST_CHECK_EQUAL(handler.store(1, 14, input2), input2.size());
      BOOST_CHECK(test_content(msg1 + std::string(" that overlaps"), tid2, 1, 0));

      // Second message that oerlaps with first one.
      std::string msg3("overlaps other stuff");
      elle::Buffer input3(msg3.c_str(), msg3.size());
      BOOST_CHECK_EQUAL(handler.store(1, 27, input3), input3.size());
      BOOST_CHECK(test_content(msg1 + std::string(" that overlaps other stuff"), tid2, 1, 0));
    }

    // Test prepend function.
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid3);

      // First simple message.
      std::string msg1("... after the other message.");
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(1, 19, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid3, 1, 19));

      // Preprend first simple message.
      std::string msg2("This is");
      elle::Buffer input2(msg2.c_str(), msg2.size());
      BOOST_CHECK_EQUAL(handler.store(1, 12, input2), input2.size());
      BOOST_CHECK(test_content(msg2 + msg1, tid3, 1, 12));

      // Preprend anoter message.
      std::string msg3("My message: ");
      elle::Buffer input3(msg3.c_str(), msg3.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input3), input3.size());
      BOOST_CHECK(test_content(msg3 + msg2 + msg1, tid3, 1, 0));
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}
#if 0
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

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(test_multiple_chunks)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction3");

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
    std::string msg3("This message is designed to be longer than others.");
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

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(test_filenotfound)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction4");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);
    handler.ident(tid);

    BOOST_CHECK_THROW(handler.fetch(1, 1), elle::Exception);

    // Store first file.
    std::string msg1("This is a common file content. ab cd-ef_gh");
    elle::Buffer input1(msg1.c_str(), msg1.size());
    BOOST_CHECK_EQUAL(handler.store(1, 1, input1), input1.size());

    // Try to fetch inexisting file.
    BOOST_CHECK_THROW(handler.fetch(1, 2), elle::Exception);

    // Store second file.
    std::string msg2("This is another common file chunk or whatever");
    elle::Buffer input2(msg2.c_str(), msg2.size());
    BOOST_CHECK_EQUAL(handler.store(1, 2, input2), input2.size());

    // Fetch existing files.
    BOOST_CHECK_NO_THROW(handler.fetch(1, 1));
    BOOST_CHECK_NO_THROW(handler.fetch(1, 2));
    BOOST_CHECK_EQUAL(handler.fetch(1, 1).size(), input1.size());
    BOOST_CHECK_EQUAL(handler.fetch(1, 2).size(), input2.size());

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(test_file_already_present)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction5");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, "127.0.0.1", port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);
    handler.ident(tid);

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

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(test_bad_basepath)
{
  reactor::Scheduler sched;

  auto tmp_server = [=] ()
  {
    auto& sched = *reactor::Scheduler::scheduler();
    BOOST_CHECK_THROW(oracle::hermes::Hermes herm(sched, port, "/dev/null"),
                      elle::Exception);
  };

  reactor::Thread serv(sched, "hermes", tmp_server);

  sched.run();
}

BOOST_AUTO_TEST_CASE(test_restart)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction6");

  auto tmp_server = [=] (reactor::Thread* to_wait)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    sched.current()->wait(*to_wait);

    oracle::hermes::Hermes(sched, port, base_path).run();
  };

  auto first_client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();
    reactor::network::TCPSocket socket(sched, host, port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);
    handler.ident(tid);

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

    reactor::network::TCPSocket socket(sched, host, port);
    infinit::protocol::Serializer s(sched, socket);
    infinit::protocol::ChanneledStream channels(sched, s);

    oracle::hermes::HermesRPC handler(channels);
    handler.ident(tid);

    BOOST_CHECK_NO_THROW(handler.fetch(5, 1));
    // 32 is the size of the previously stored file.
    BOOST_CHECK_EQUAL(handler.fetch(5, 1).size(), 32);

    serv->terminate_now();
  };

  reactor::Thread serv1(sched, "hermes1", server);
  reactor::Thread cli1(sched, "client1", std::bind(first_client, &serv1));

  reactor::Thread serv2(sched, "hermes2", std::bind(tmp_server, &cli1));

  reactor::Thread cli2(sched, "client2", std::bind(second_client,
                                                   &cli1,
                                                   &serv2));

  sched.run();
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

  reactor::Thread serv1(sched, "hermes1", first_server);
  reactor::Thread cli1(sched, "client1", std::bind(first_client, &serv1));

  reactor::Thread serv2(sched, "hermes2", std::bind(second_server, &cli1));
  reactor::Thread cli2(sched, "client2", std::bind(second_client, &cli1, &serv2));

  sched.run();
  //clean(base_path);
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
  //clean(base_path);
}
#endif
