#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include <reactor/scheduler.hh>
#include <elle/serialize/BinaryArchive.hh>

#include <reactor/network/exception.hh>
#include <reactor/network/tcp-server.hh>
#include <reactor/scheduler.hh>
#include <reactor/semaphore.hh>
#include <reactor/thread.hh>

#include <protocol/ChanneledStream.hh>
#include <protocol/Serializer.hh>
#include <protocol/RPC.hh>

int answer()
{
  return 42;
}

int answer2()
{
  return 42;
}

int square(int x)
{
  return x * x;
}

std::string concat(std::string const& left, std::string const& right)
{
  return left + right;
}

reactor::Thread* suicide_thread(nullptr);
void suicide()
{
  suicide_thread->terminate();
  suicide_thread = nullptr;
  reactor::Scheduler::scheduler()->current()->yield();
  reactor::Scheduler::scheduler()->current()->yield();
  BOOST_CHECK(false);
}

static int global_counter = 0;
int count(int exp)
{
  ++global_counter;
  reactor::Scheduler::scheduler()->current()->sleep(
    boost::posix_time::milliseconds(10));
  BOOST_CHECK_EQUAL(global_counter, exp);
  return global_counter;
}

void except()
{
  throw std::runtime_error("blablabla");
}

struct DummyRPC: public infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                                               elle::serialize::OutputBinaryArchive>
{
  DummyRPC(infinit::protocol::ChanneledStream& channels)
    : infinit::protocol::RPC<elle::serialize::InputBinaryArchive,
                             elle::serialize::OutputBinaryArchive>(channels)
    , answer("answer", *this)
    , square("square", *this)
    , concat("concat", *this)
    , raise("raise", *this)
    , suicide("suicide", *this)
    , count("count", *this)
  {}

  RemoteProcedure<int> answer;
  RemoteProcedure<int, int> square;
  RemoteProcedure<std::string, std::string const&, std::string const&> concat;
  RemoteProcedure<void> raise;
  RemoteProcedure<void> suicide;
  RemoteProcedure<int, int> count;
};

/*------.
| Basic |
`------*/

void caller(reactor::Semaphore& lock)
{
  auto& sched = *reactor::Scheduler::scheduler();
  sched.current()->wait(lock);
  reactor::network::TCPSocket socket(sched, "127.0.0.1", 12345);
  infinit::protocol::Serializer s(sched, socket);
  infinit::protocol::ChanneledStream channels(sched, s);

  DummyRPC rpc(channels);
  BOOST_CHECK_EQUAL(rpc.answer(), 42);
  BOOST_CHECK_EQUAL(rpc.square(8), 64);
  BOOST_CHECK_EQUAL(rpc.concat("foo", "bar"), "foobar");
  BOOST_CHECK_THROW(rpc.raise(), std::runtime_error);
}

void runner(reactor::Semaphore& lock,
            bool sync)
{
  auto& sched = *reactor::Scheduler::scheduler();
  reactor::network::TCPServer server(sched);
  server.listen(12345);
  lock.release();
  lock.release();
  reactor::network::TCPSocket* socket = server.accept();
  infinit::protocol::Serializer s(sched, *socket);
  infinit::protocol::ChanneledStream channels(sched, s);

  DummyRPC rpc(channels);
  rpc.answer = &answer;
  rpc.square = &square;
  rpc.concat = &concat;
  rpc.raise  = &except;
  rpc.suicide  = &suicide;
  rpc.count  = &count;
  try
  {
    if (sync)
      rpc.run();
    else
      rpc.parallel_run();
  }
  catch (reactor::network::ConnectionClosed&)
  {}
}

int test(bool sync)
{
  reactor::Scheduler sched;
  reactor::Semaphore lock;

  reactor::Thread r(sched, "Runner", std::bind(runner, std::ref(lock), sync));
  reactor::Thread c(sched, "Caller", std::bind(caller, std::ref(lock)));

  sched.run();
  return 0;
}

/*----------.
| Terminate |
`----------*/

void pacify(reactor::Semaphore& lock, reactor::Thread& t)
{
  auto& sched = *reactor::Scheduler::scheduler();
  sched.current()->wait(lock);
  reactor::network::TCPSocket socket(sched, "127.0.0.1", 12345);
  infinit::protocol::Serializer s(sched, socket);
  infinit::protocol::ChanneledStream channels(sched, s);

  DummyRPC rpc(channels);
  suicide_thread = &t;
  BOOST_CHECK_THROW(rpc.suicide(), std::runtime_error);
}

int test_terminate(bool sync)
{
  reactor::Scheduler sched{};
  reactor::Semaphore lock;

  reactor::Thread r(sched, "Runner", std::bind(runner,
                                               std::ref(lock),
                                               sync));
  reactor::Thread j(sched, "Judge dread", std::bind(pacify,
                                                    std::ref(lock),
                                                    std::ref(r)));

  sched.run();
  return 0;
}

/*---------.
| Parallel |
`---------*/

void counter(reactor::Semaphore& lock, bool sync)
{
  auto& sched = *reactor::Scheduler::scheduler();
  sched.current()->wait(lock);
  reactor::network::TCPSocket socket(sched, "127.0.0.1", 12345);
  infinit::protocol::Serializer s(sched, socket);
  infinit::protocol::ChanneledStream channels(sched, s);

  DummyRPC rpc(channels);
  global_counter = 0;
  std::vector<reactor::Thread*> threads;
  for (int i = 1; i <= 3; ++i)
  {
    auto t = new reactor::Thread(sched,
                                 elle::sprintf("Counter %s", i),
                                 [&,i] ()
                                 {
                                   rpc.count(sync ? i : 3);
                                 });
    threads.push_back(t);
  }
  for (auto& t: threads)
  {
    sched.current()->wait(*t);
    delete t;
  }
}


int test_parallel(bool sync)
{
  reactor::Scheduler sched;
  reactor::Semaphore lock;

  reactor::Thread r(sched, "Runner", std::bind(runner,
                                               std::ref(lock),
                                               sync));
  reactor::Thread c1(sched, "Counter", std::bind(counter,
                                                   std::ref(lock),
                                                   sync));

  sched.run();
  return 0;
}

/*-----------.
| Test suite |
`-----------*/

bool test_suite()
{
  boost::unit_test::test_suite* rpc = BOOST_TEST_SUITE("RPC");
  boost::unit_test::framework::master_test_suite().add(rpc);
  rpc->add(BOOST_TEST_CASE(std::bind(test, true)));
  rpc->add(BOOST_TEST_CASE(std::bind(test, false)));
  rpc->add(BOOST_TEST_CASE(std::bind(test_terminate, true)));
  rpc->add(BOOST_TEST_CASE(std::bind(test_terminate, false)));
  rpc->add(BOOST_TEST_CASE(std::bind(test_parallel, true)));
  rpc->add(BOOST_TEST_CASE(std::bind(test_parallel, false)));
  return true;
}

int
main(int argc, char** argv)
{
  return ::boost::unit_test::unit_test_main(test_suite, argc, argv);
}
