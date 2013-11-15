#define BOOST_TEST_MODULE Apertus

#include <boost/test/unit_test.hpp>

#include <reactor/duration.hh>
#include <infinit/oracles/apertus/Apertus.hh>

#if 0
BOOST_AUTO_TEST_CASE(basic)
{
  reactor::Scheduler sched;
  std::unique_ptr<oracles::apertus::Apertus> ap; 

  reactor::Thread main(sched, "main", [&]
    {
      ap.reset(new oracles::apertus::Apertus(sched, "", 0, "127.0.0.1", 6565));

      main.wait(*ap);
      ap.reset();
    });

  auto client = [&]
  {
    reactor::network::TCPSocket sock("127.0.0.1", 6565);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) != haha.end());
    BOOST_CHECK(haha[std::string(tid)] != nullptr);

    ap->stop();
  };

  new reactor::Thread(sched, "client", client);

  sched.run();
}
#endif

BOOST_AUTO_TEST_CASE(one_way_exchange)
{
  reactor::Scheduler sched;
  std::unique_ptr<oracles::apertus::Apertus> ap; 

  reactor::Thread main(sched, "main", [&]
    {
      ap.reset(new oracles::apertus::Apertus(sched, "", 0, "127.0.0.1", 6566));

      main.wait(*ap);
      ap.reset();
    });

  auto client1 = [&]
  {
    reactor::sleep(1_sec);

    std::cout << "first client b4 conn" << std::endl;
    reactor::network::TCPSocket sock("127.0.0.1", 6566);

    std::cout << "first client connects" << std::endl;

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) != haha.end());
    BOOST_CHECK(haha[std::string(tid)] != nullptr);

    const char* msg1 = "firt_message";
    elle::ConstWeakBuffer msg_buffer1(msg1, 13);
    sock.write(msg_buffer1);
  };

  auto client2 = [&]
  {
    reactor::sleep(1_sec);

    std::cout << "second client b4 conn" << std::endl;
    reactor::network::TCPSocket sock("127.0.0.1", 6566);

    std::cout << "second client connects" << std::endl;

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    char content[13];
    reactor::network::Buffer tmp(content, 13);
    sock.read(tmp);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) == haha.end());
    BOOST_CHECK_EQUAL(std::string(content), std::string("firt_message"));

    ap->stop();
  };

  new reactor::Thread(sched, "client1", client1);
  new reactor::Thread(sched, "client2", client2);

  sched.run();
}

#if 0
BOOST_AUTO_TEST_CASE(two_ways_exchange)
{
  reactor::Scheduler sched;
  std::unique_ptr<oracles::apertus::Apertus> ap; 

  reactor::Thread main(sched, "main", [&]
    {
      ap.reset(new oracles::apertus::Apertus(sched, "", 0, "127.0.0.1", 6567));

      main.wait(*ap);
      ap.reset();
    });

  auto client1 = [&]
  {
    reactor::network::TCPSocket sock("127.0.0.1", 6567);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) != haha.end());
    BOOST_CHECK(haha[std::string(tid)] != nullptr);

    const char* msg1 = "firt_message";
    elle::ConstWeakBuffer msg_buffer1(msg1, 13);
    sock.write(msg_buffer1);

    char content[6];
    reactor::network::Buffer tmp(content, 6);
    sock.read(tmp);

    BOOST_CHECK_EQUAL(std::string(content), std::string("hitler"));

    ap->stop();
  };

  auto client2 = [&]
  {
    reactor::sleep(1_sec);
    reactor::network::TCPSocket sock("127.0.0.1", 6567);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    char content[13];
    reactor::network::Buffer tmp(content, 13);
    sock.read(tmp);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) == haha.end());
    BOOST_CHECK_EQUAL(std::string(content), std::string("firt_message"));

    const char* msg1 = "hitler";
    elle::ConstWeakBuffer msg_buffer1(msg1, 6);
    sock.write(msg_buffer1);
  };

  new reactor::Thread(sched, "client1", client1);
  new reactor::Thread(sched, "client2", client2);

  sched.run();
}

BOOST_AUTO_TEST_CASE(two_ways_exchange_big)
{
  reactor::Scheduler sched;
  std::unique_ptr<oracles::apertus::Apertus> ap; 

  reactor::Thread main(sched, "main", [&]
    {
      ap.reset(new oracles::apertus::Apertus(sched, "", 0, "127.0.0.1", 6568));

      main.wait(*ap);
      ap.reset();
    });

  auto client1 = [&]
  {
    reactor::network::TCPSocket sock("127.0.0.1", 6568);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) != haha.end());
    BOOST_CHECK(haha[std::string(tid)] != nullptr);

    const char* msg1 = "firt_message";
    elle::ConstWeakBuffer msg_buffer1(msg1, 13);
    sock.write(msg_buffer1);

    char content[7] = { 0 };

    reactor::network::Buffer tmp(content, 6);
    sock.read(tmp);

    BOOST_CHECK_EQUAL(std::string(content), std::string("hitler"));

    char content2[12] = { 0 };
    reactor::network::Buffer tmp2(content2, 11);
    sock.read(tmp2);

    BOOST_CHECK_EQUAL(std::string(content2), std::string("BIG_CONTENT"));

    ap->stop();
  };

  auto client2 = [&]
  {
    reactor::sleep(1_sec);
    reactor::network::TCPSocket sock("127.0.0.1", 6568);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    reactor::sleep(1_sec);

    char content[13];
    reactor::network::Buffer tmp(content, 13);
    sock.read(tmp);

    auto& haha = ap->get_clients();
    BOOST_CHECK(haha.find(std::string(tid)) == haha.end());
    BOOST_CHECK_EQUAL(std::string(content), std::string("firt_message"));

    const char* msg1 = "hitler";
    elle::ConstWeakBuffer msg_buffer1(msg1, 6);
    sock.write(msg_buffer1);

    const char* msg2 = "BIG_CONTENT";
    elle::ConstWeakBuffer msg_buffer2(msg2, 11);
    sock.write(msg_buffer2);
  };

  new reactor::Thread(sched, "client1", client1);
  new reactor::Thread(sched, "client2", client2);

  sched.run();
}
#endif
