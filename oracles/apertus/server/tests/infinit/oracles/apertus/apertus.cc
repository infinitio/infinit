#define BOOST_TEST_MODULE Apertus

#include <boost/test/unit_test.hpp>

#include <infinit/oracles/apertus/Apertus.hh>

BOOST_AUTO_TEST_CASE(basic)
{
  reactor::Scheduler sched;
  oracles::apertus::Apertus* ap = new oracles::apertus::Apertus(sched, "", 0, "127.0.0.1", 6565);

  auto client = [&] (reactor::Thread* serv)
  {
    reactor::network::TCPSocket sock("127.0.0.1", 6565);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    sched.current()->yield();

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) != haha.end());
    BOOST_CHECK(haha[std::string(tid)] != nullptr);

    serv->terminate_now();
  };

  reactor::Thread* serv = new reactor::Thread(sched, "serv",
    std::bind(&oracles::apertus::Apertus::run, ap));
  new reactor::Thread(sched, "client", std::bind(client, serv));

  sched.run();
  delete ap;
}

BOOST_AUTO_TEST_CASE(exchange)
{
  reactor::Scheduler sched;
  oracles::apertus::Apertus* ap = new oracles::apertus::Apertus(sched, "", 0, "127.0.0.1", 6566);

  auto client1 = [&] (reactor::Thread* serv)
  {
    reactor::network::TCPSocket sock("127.0.0.1", 6566);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    sched.current()->yield();

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) != haha.end());
    BOOST_CHECK(haha[std::string(tid)] != nullptr);

    const char* msg1 = "firt_message";
    elle::ConstWeakBuffer msg_buffer1(msg1, 13);
    sock.write(msg_buffer1);
  };

  auto client2 = [&] (reactor::Thread* serv)
  {
    sched.current()->yield();
    reactor::network::TCPSocket sock("127.0.0.1", 6566);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    sched.current()->yield();

    char content[13];
    reactor::network::Buffer tmp(content, 13);
    sock.read(tmp);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) == haha.end());
    BOOST_CHECK_EQUAL(std::string(content), std::string("firt_message"));

    serv->terminate_now();
  };

  reactor::Thread* serv = new reactor::Thread(sched, "serv",
    std::bind(&oracles::apertus::Apertus::run, ap));

  new reactor::Thread(sched, "client1", std::bind(client1, serv));
  new reactor::Thread(sched, "client2", std::bind(client2, serv));

  sched.run();
  delete ap;
}
