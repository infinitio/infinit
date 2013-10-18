#define BOOST_TEST_MODULE Hermes

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <reactor/scheduler.hh>
#include <elle/Exception.hh>

#include <hermes/Hermes.hh>

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

  std::cout << msg << ":" << std::string(output) << std::endl;

  bool result(std::string(output) == msg);
  delete output;
  return result;
}

BOOST_AUTO_TEST_CASE(append)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction1");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // First simple message.
      std::string msg1("This is basic content");
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 1, 0));

      // Second message that oerlaps with first one.
      std::string msg2("content that overlaps");
      elle::Buffer input2(msg2.c_str(), msg2.size());
      BOOST_CHECK_EQUAL(handler.store(1, 14, input2), input2.size());
      BOOST_CHECK(test_content(msg1 + std::string(" that overlaps"), tid, 1, 0));

      // Second message that oerlaps with first one.
      std::string msg3("overlaps other stuff");
      elle::Buffer input3(msg3.c_str(), msg3.size());
      BOOST_CHECK_EQUAL(handler.store(1, 27, input3), input3.size());
      BOOST_CHECK(test_content(msg1 + std::string(" that overlaps other stuff"), tid, 1, 0));
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(merge)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction2");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // First simple message.
      std::string msg1("This is basic content");
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 1, 0));

      // Second message that oerlaps with first one.
      std::string msg2("content that overlaps");
      elle::Buffer input2(msg2.c_str(), msg2.size());
      BOOST_CHECK_EQUAL(handler.store(1, 14, input2), input2.size());
      BOOST_CHECK(test_content(msg1 + std::string(" that overlaps"), tid, 1, 0));

      // Second message that oerlaps with first one.
      std::string msg3("overlaps other stuff");
      elle::Buffer input3(msg3.c_str(), msg3.size());
      BOOST_CHECK_EQUAL(handler.store(1, 27, input3), input3.size());
      BOOST_CHECK(test_content(msg1 + std::string(" that overlaps other stuff"), tid, 1, 0));
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(prepend)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid3("transaction3");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

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

BOOST_AUTO_TEST_CASE(mix)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction4");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    // Mega mix!
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // This |is a message that |is pretty |cool, |isn't it?|
      // _____|__________________|_________________|
      //   5        1                   2   |________________|
      //                                              4

      // First simple message.
      std::string msg1("is a message that ");
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(0, 5, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 0, 5));

      // Push another piece of the message
      std::string msg2("is pretty cool, ");
      elle::Buffer input2(msg2.c_str(), msg2.size());
      BOOST_CHECK_EQUAL(handler.store(0, 23, input2), input2.size());
      BOOST_CHECK(test_content(msg1 + msg2, tid, 0, 5));

      // Push an independent message
      std::string msg3("I am an independ message");
      elle::Buffer input3(msg3.c_str(), msg3.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input3), input3.size());
      BOOST_CHECK(test_content(msg3, tid, 1, 0));

      std::string msg4("cool, isn't it?");
      elle::Buffer input4(msg4.c_str(), msg4.size());
      BOOST_CHECK_EQUAL(handler.store(0, 33, input4), input4.size());
      BOOST_CHECK(test_content(msg1 + "is pretty " + msg4, tid, 0, 5));

      // Prepend the beginning of the message
      std::string msg5("This ");
      elle::Buffer input5(msg5.c_str(), msg5.size());
      BOOST_CHECK_EQUAL(handler.store(0, 0, input5), input5.size());
      BOOST_CHECK(test_content(msg5 + msg1 + "is pretty " + msg4, tid, 0, 0));
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(simple_fetch)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction5");
  std::string msg1("This is basic content");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // First simple message.
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(1, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 1, 0));

      // Retrieval of other message directly after.
      elle::Buffer output1(handler.fetch(1, 0, msg1.size()));
    }

    // Retrieval of message after reconnexion from another place.
    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // Retrieval of other message directly after.
      elle::Buffer output1(handler.fetch(1, 0, msg1.size()));
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(partial_fetch)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction6");
  std::string msg1("content");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // First simple message.
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(0, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 0, 0));

      // Retrieval of message directly after.
      elle::Buffer output1;
      BOOST_CHECK_NO_THROW(output1 = handler.fetch(0, 0, msg1.size() + 2));
      BOOST_CHECK_EQUAL(output1.size(), msg1.size());
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(fail_fetch)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction7");
  std::string msg1("content");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // First simple message.
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(0, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 0, 0));

      // Try to retrieve unexisting message.
      elle::Buffer output1;
      BOOST_CHECK_THROW(output1 = handler.fetch(1, 0, msg1.size()),
                        elle::Exception);
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(fail_store)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction8");
  std::string msg1("content");

  auto client = [=] (reactor::Thread* serv)
  {
    auto& sched = *reactor::Scheduler::scheduler();

    {
      reactor::network::TCPSocket socket(sched, host, port);
      infinit::protocol::Serializer s(sched, socket);
      infinit::protocol::ChanneledStream channels(sched, s);

      oracle::hermes::HermesRPC handler(channels);
      handler.ident(tid);

      // First simple message.
      elle::Buffer input1(msg1.c_str(), msg1.size());
      BOOST_CHECK_EQUAL(handler.store(0, 0, input1), input1.size());
      BOOST_CHECK(test_content(msg1, tid, 0, 0));

      // Try to store the message once again with the same id.
      elle::Buffer input2(msg1.c_str(), msg1.size());
      BOOST_CHECK_THROW(handler.store(0, 0, input1), elle::Exception);
      BOOST_CHECK(test_content(msg1, tid, 0, 0));
    }

    serv->terminate_now();
  };

  reactor::Thread serv(sched, "hermes", server);
  reactor::Thread cli(sched, "client", std::bind(client, &serv));

  sched.run();
}

BOOST_AUTO_TEST_CASE(fail_boot)
{
  reactor::Scheduler sched;

  oracle::hermes::TID tid("transaction9");

  auto serv2 = [=]
  {
    auto& sched = *reactor::Scheduler::scheduler();
    BOOST_CHECK_THROW(oracle::hermes::Hermes(sched, port, "/dev/null").run(),
                      elle::Exception);
  };

  reactor::Thread serv(sched, "hermes", serv2);
  sched.run();
}
