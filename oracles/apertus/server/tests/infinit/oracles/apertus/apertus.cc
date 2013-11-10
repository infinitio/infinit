#define BOOST_TEST_MODULE Apertus

#include <boost/test/unit_test.hpp>

#include <infinit/oracles/apertus/Apertus.hh>

BOOST_AUTO_TEST_CASE(basic)
{
  reactor::Scheduler sched;
  oracles::apertus::Apertus ap(sched, "", 0, "127.0.0.1", 6565);

  auto client = [&] (reactor::Thread* serv)
  {
    reactor::network::TCPSocket sock("127.0.0.1", 6565);

    char tidsize = 6;
    elle::ConstWeakBuffer sizebuffer(&tidsize, 1);
    sock.write(sizebuffer);

    const char* tid = "hitler";
    elle::ConstWeakBuffer tidbuffer(tid, tidsize);
    sock.write(tidbuffer);

    
  };

  reactor::Thread* serv = new reactor::Thread(sched, "serv",
    std::bind(&oracles::apertus::Apertus::run, ap));
  new reactor::Thread(sched, "client", std::bind(client, serv));

  sched.run();
}
