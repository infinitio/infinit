#include "test.hh"
#include "backend/test.hh"
#include "network/test.hh"

#include <reactor/semaphore.hh>
#include <reactor/mutex.hh>
#include <reactor/rw-mutex.hh>
#include <reactor/signal.hh>
#include <reactor/storage.hh>
#include <reactor/thread.hh>

/*-----------------.
| Global shortcuts |
`-----------------*/

reactor::Scheduler* sched = 0;

Fixture::Fixture()
{
  sched = new reactor::Scheduler;
}

Fixture::~Fixture()
{
  delete sched;
  sched = 0;
}

void yield()
{
  sched->current()->yield();
}

bool wait(reactor::Waitable& s,
          reactor::DurationOpt timeout = reactor::DurationOpt())
{
  return sched->current()->wait(s, timeout);
}

void wait(reactor::Waitables& s)
{
  sched->current()->wait(s);
}

void sleep(reactor::Duration d)
{
  sched->current()->sleep(d);
}

/*--------.
| Helpers |
`--------*/

void empty()
{}

/*-------.
| Basics |
`-------*/

void coro(int& step)
{
  BOOST_CHECK_EQUAL(step, 0);
  ++step;
  yield();
  BOOST_CHECK_EQUAL(step, 1);
  ++step;
}

void test_basics_one()
{
  Fixture f;

  int step = 0;
  reactor::Thread t(*sched, "coro", boost::bind(coro, boost::ref(step)));
  sched->run();
  BOOST_CHECK_EQUAL(step, 2);
}

void coro1(int& step)
{
  BOOST_CHECK(step == 0 || step == 1);
  ++step;
  yield();
  BOOST_CHECK(step == 2 || step == 3);
  ++step;
  yield();
  BOOST_CHECK(step == 4);
  ++step;
  yield();
  BOOST_CHECK(step == 5);
}

void coro2(int& step)
{
  BOOST_CHECK(step == 0 || step == 1);
  ++step;
  yield();
  BOOST_CHECK(step == 2 || step == 3);
  ++step;
}

void test_basics_interleave()
{
  Fixture f;

  int step = 0;
  reactor::Thread c1(*sched, "1", boost::bind(coro1, boost::ref(step)));
  reactor::Thread c2(*sched, "2", boost::bind(coro2, boost::ref(step)));
  sched->run();
  BOOST_CHECK_EQUAL(step, 5);
}

/*--------.
| Signals |
`--------*/

void waiter(int& step, reactor::Waitables& waitables)
{
  BOOST_CHECK_EQUAL(step, 0);
  sched->current()->wait(waitables);
  ++step;
}

void sender_one(int& step, reactor::Signal& s, int expect)
{
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  BOOST_CHECK_EQUAL(step, 0);
  s.signal();
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  yield();
  BOOST_CHECK_EQUAL(step, expect);
}

void sender_two(int& step, reactor::Signal& s1, reactor::Signal& s2)
{
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  BOOST_CHECK_EQUAL(step, 0);
  s1.signal();
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  BOOST_CHECK_EQUAL(step, 0);
  yield();
  s2.signal();
  yield();
  yield();
  BOOST_CHECK_EQUAL(step, 1);
}

void test_signals_one_on_one()
{
  Fixture f;

  int step = 0;
  reactor::Signal signal;
  reactor::Waitables signals;
  signals << signal;
  reactor::Thread w(*sched, "waiter",
                    boost::bind(waiter, boost::ref(step), signals));
  reactor::Thread s(*sched, "sender",
                    boost::bind(sender_one, boost::ref(step),
                                boost::ref(signal), 1));
  sched->run();
  BOOST_CHECK_EQUAL(step, 1);
}

void test_signals_one_on_two()
{
  Fixture f;

  int step = 0;
  reactor::Signal signal1;
  reactor::Signal signal2;
  reactor::Waitables signals;
  signals << signal1 << signal2;
  reactor::Thread w(*sched, "waiter",
                    boost::bind(waiter, boost::ref(step), signals));
  reactor::Thread s(*sched, "sender",
                    boost::bind(sender_two, boost::ref(step),
                                boost::ref(signal1), boost::ref(signal2)));
  sched->run();
  BOOST_CHECK_EQUAL(step, 1);
}

void test_signals_two_on_one()
{
  Fixture f;

  int step = 0;
  reactor::Signal signal;
  reactor::Waitables signals;
  signals << signal;
  reactor::Thread w1(*sched, "waiter1",
                     boost::bind(waiter, boost::ref(step), signals));
  reactor::Thread w2(*sched, "waiter2",
                     boost::bind(waiter, boost::ref(step), signals));
  reactor::Thread s(*sched, "sender",
                    boost::bind(sender_one, boost::ref(step),
                                boost::ref(signal), 2));
  sched->run();
  BOOST_CHECK_EQUAL(step, 2);
}

void waiter_timeout()
{
  reactor::Signal s;
  bool finished = wait(s, boost::posix_time::milliseconds(10));
  BOOST_CHECK(!finished);
  s.signal();
}

void test_signals_timeout()
{
  Fixture f;

  reactor::Thread t(*sched, "waiter", waiter_timeout);
  sched->run();
}

/*------.
| Sleep |
`------*/

void sleeper1(int& step)
{
  BOOST_CHECK(step == 0 || step == 1);
  ++step;
  sleep(boost::posix_time::milliseconds(400));
  BOOST_CHECK_EQUAL(step, 3);
  ++step;
}

void sleeper2(int& step)
{
  BOOST_CHECK(step == 0 || step == 1);
  ++step;
  sleep(boost::posix_time::milliseconds(200));
  BOOST_CHECK_EQUAL(step, 2);
  ++step;
}

void test_sleep_interleave()
{
  Fixture f;

  int step = 0;
  reactor::Thread s1(*sched, "sleeper1",
                     boost::bind(sleeper1, boost::ref(step)));
  reactor::Thread s2(*sched, "sleeper2",
                     boost::bind(sleeper2, boost::ref(step)));
  sched->run();
}

static boost::posix_time::ptime now()
{
  return boost::posix_time::microsec_clock::local_time();
}

void sleep_timer(int& iterations)
{
  reactor::Duration delay(boost::posix_time::milliseconds(100));

  while (--iterations)
  {
    boost::posix_time::ptime start(now());
    sleep(delay);
    double elapsed = (now() - start).total_milliseconds();
    double expected =  delay.total_milliseconds();
    BOOST_CHECK_CLOSE(elapsed, expected, double(10));
  }
}

void test_sleep_timing()
{
  Fixture f;

  int iterations = 8;
  reactor::Thread t(*sched, "sleeper",
                    boost::bind(sleep_timer, boost::ref(iterations)));
  sched->run();
  BOOST_CHECK_EQUAL(iterations, 0);
}

/*-----.
| Join |
`-----*/

void joined(int& count)
{
  BOOST_CHECK_EQUAL(count, 0);
  yield();
  ++count;
  yield();
  yield();
  ++count;
}

void join_waiter(reactor::Thread& thread, int& count)
{
  wait(thread);
  BOOST_CHECK_EQUAL(count, 2);
  ++count;
}

void test_join()
{
  Fixture f;

  int count = 0;
  reactor::Thread j(*sched, "joined",
                    boost::bind(joined, boost::ref(count)));
  reactor::Thread w(*sched, "waiter",
                    boost::bind(join_waiter,
                                boost::ref(j), boost::ref(count)));
  sched->run();
  BOOST_CHECK_EQUAL(count, 3);
}

void join_waiter_multiple(reactor::Thread& thread, int& count)
{
  yield();
  BOOST_CHECK(thread.state() == reactor::Thread::state::done);
  wait(thread);
  wait(thread);
  ++count;
}

void test_join_multiple()
{
  Fixture f;

  int count = 0;
  reactor::Thread e(*sched, "empty", empty);
  reactor::Thread w(*sched, "waiter",
                    boost::bind(join_waiter_multiple,
                                boost::ref(e), boost::ref(count)));
  sched->run();
  BOOST_CHECK_EQUAL(count, 1);
}

void sleeping_beauty()
{
  sleep(boost::posix_time::milliseconds(100));
}

void prince_charming(reactor::Thread& sleeping_beauty)
{
  bool finished = wait(sleeping_beauty, boost::posix_time::milliseconds(50));
  BOOST_CHECK(!finished);
  BOOST_CHECK(!sleeping_beauty.done());
  finished = wait(sleeping_beauty, boost::posix_time::milliseconds(200));
  BOOST_CHECK(finished);
  BOOST_CHECK(sleeping_beauty.done());
}

void test_join_timeout()
{
  Fixture f;

  reactor::Thread sb(*sched, "sleeping beauty", sleeping_beauty);
  reactor::Thread pc(*sched, "prince charming",
                     boost::bind(prince_charming, boost::ref(sb)));
  sched->run();
}

/*--------.
| Timeout |
`--------*/

void timeout(reactor::Signal& s, bool expect)
{
  bool finished = wait(s, boost::posix_time::milliseconds(500));
  BOOST_CHECK(finished == expect);
  BOOST_CHECK(s.waiters().empty());
}

void timeout_send(reactor::Signal& s)
{
  yield();
  BOOST_CHECK_EQUAL(s.waiters().size(), 1);
  s.signal();
}

void test_timeout_do()
{
  Fixture f;

  reactor::Signal s;
  reactor::Thread t(*sched, "timeout",
                    boost::bind(timeout, boost::ref(s), false));
  sched->run();
}

void test_timeout_dont()
{
  Fixture f;

  reactor::Signal s;
  reactor::Thread t(*sched, "timeout",
                    boost::bind(timeout, boost::ref(s), true));
  reactor::Thread p(*sched, "poker",
                    boost::bind(timeout_send, boost::ref(s)));
  sched->run();
}

/*--------.
| VThread |
`--------*/

int answer()
{
  return 42;
}

void test_vthread()
{
  Fixture f;

  reactor::VThread<int> t(*sched, "return value", answer);
  BOOST_CHECK_THROW(t.result(), elle::Exception);
  sched->run();
  BOOST_CHECK_EQUAL(t.result(), 42);
}

/*------------.
| Multithread |
`------------*/

void waker(reactor::Signal& s)
{
  // FIXME: sleeps suck

  // Make sure the scheduler is sleeping.
  sleep(1);
  reactor::Thread w(*sched, "waker",
                    boost::bind(&reactor::Signal::signal, &s));
  // Make sure the scheduler is done.
  sleep(1);
}

void test_multithread_spawn_wake()
{
  Fixture f;
  reactor::Signal sig;
  typedef bool (reactor::Thread::*F)(reactor::Waitable&, reactor::DurationOpt);
  reactor::Thread keeper(*sched, "keeper",
                         boost::bind(static_cast<F>(&reactor::Thread::wait),
                                     &keeper, boost::ref(sig),
                                     reactor::DurationOpt()));
  boost::thread s(boost::bind(waker, boost::ref(sig)));
  sched->run();
  s.join();
}

int spawned(reactor::Signal& s)
{
  s.signal();
  return 42;
}

void spawn(reactor::Signal& s, int& res)
{
  res = sched->mt_run<int>("spawned", boost::bind(spawned, boost::ref(s)));
}

void spawner()
{
  reactor::Signal s;
  int res = 0;
  boost::thread spawner(boost::bind(spawn, boost::ref(s), boost::ref(res)));
  wait(s);
  spawner.join();
  BOOST_CHECK_EQUAL(res, 42);
}

void test_multithread_run()
{
  Fixture f;

  reactor::Thread t(*sched, "spawner", spawner);
  sched->run();
}

/*----------.
| Semaphore |
`----------*/

void semaphore_noblock_wait(reactor::Semaphore& s)
{
  BOOST_CHECK_EQUAL(s.count(), 2);
  wait(s);
  BOOST_CHECK_EQUAL(s.count(), 1);
  wait(s);
  BOOST_CHECK_EQUAL(s.count(), 0);
}

void test_semaphore_noblock()
{
  Fixture f;
  reactor::Semaphore s(2);
  reactor::Thread wait(*sched, "wait",
                       boost::bind(&semaphore_noblock_wait, boost::ref(s)));
  sched->run();
}

void semaphore_block_wait(reactor::Semaphore& s)
{
  BOOST_CHECK_EQUAL(s.count(), 0);
  wait(s);
  BOOST_CHECK_EQUAL(s.count(), 0);
}

void semaphore_block_post(reactor::Semaphore& s)
{
  yield();
  yield();
  yield();
  BOOST_CHECK_EQUAL(s.count(), -1);
  s.release();
  BOOST_CHECK_EQUAL(s.count(), 0);
}

void test_semaphore_block()
{
  Fixture f;
  reactor::Semaphore s;
  reactor::Thread wait(*sched, "wait",
                       boost::bind(&semaphore_block_wait, boost::ref(s)));
  reactor::Thread post(*sched, "post",
                       boost::bind(&semaphore_block_post, boost::ref(s)));
  sched->run();
}

void semaphore_multi_wait(reactor::Semaphore& s, int& step)
{
  wait(s);
  ++step;
}

void semaphore_multi_post(reactor::Semaphore& s, int& step)
{
  yield();
  yield();
  yield();
  BOOST_CHECK_EQUAL(s.count(), -2);
  BOOST_CHECK_EQUAL(step, 0);
  s.release();
  yield();
  BOOST_CHECK_EQUAL(s.count(), -1);
  BOOST_CHECK_EQUAL(step, 1);
  s.release();
  yield();
  BOOST_CHECK_EQUAL(s.count(), 0);
  BOOST_CHECK_EQUAL(step, 2);
}

void test_semaphore_multi()
{
  Fixture f;
  reactor::Semaphore s;
  int step = 0;
  reactor::Thread wait1(*sched, "wait1",
                       boost::bind(&semaphore_multi_wait,
                                   boost::ref(s), boost::ref(step)));
  reactor::Thread wait2(*sched, "wait2",
                       boost::bind(&semaphore_multi_wait,
                                   boost::ref(s), boost::ref(step)));
  reactor::Thread post(*sched, "post",
                       boost::bind(&semaphore_multi_post,
                                   boost::ref(s), boost::ref(step)));
  sched->run();
}

/*------.
| Mutex |
`------*/

static const int mutex_yields = 32;

void mutex_count(int& i, reactor::Mutex& mutex, int yields)
{
  int count = 0;
  int prev = -1;
  while (count < mutex_yields)
  {
    {
      reactor::Lock lock(*sched, mutex);
      // For now, mutex do guarantee fairness between lockers.
      //BOOST_CHECK_NE(i, prev);
      (void)prev;
      BOOST_CHECK_EQUAL(i % 2, 0);
      ++i;
      for (int c = 0; c < yields; ++c)
      {
        ++count;
        yield();
      }
      ++i;
      prev = i;
    }
    yield();
  }
}

void test_mutex()
{
  Fixture f;
  reactor::Mutex mutex;
  int step = 0;
  reactor::Thread c1(*sched, "counter1",
                     boost::bind(&mutex_count,
                                 boost::ref(step), boost::ref(mutex), 1));
  reactor::Thread c2(*sched, "counter2",
                     boost::bind(&mutex_count,
                                 boost::ref(step), boost::ref(mutex), 1));
  reactor::Thread c3(*sched, "counter3",
                     boost::bind(&mutex_count,
                                 boost::ref(step), boost::ref(mutex), 1));
  sched->run();
}

/*--------.
| RWMutex |
`--------*/

void rw_mutex_read(reactor::RWMutex& mutex, int& step)
{
  reactor::Lock lock(*sched, mutex);
  ++step;
  yield();
  BOOST_CHECK_EQUAL(step, 3);
}

void test_rw_mutex_multi_read()
{
  Fixture f;
  reactor::RWMutex mutex;
  int step = 0;
  reactor::Thread r1(*sched, "reader1",
                     boost::bind(rw_mutex_read,
                                 boost::ref(mutex), boost::ref(step)));
  reactor::Thread r2(*sched, "reader2",
                     boost::bind(rw_mutex_read,
                                 boost::ref(mutex), boost::ref(step)));
  reactor::Thread r3(*sched, "reader3",
                     boost::bind(rw_mutex_read,
                                 boost::ref(mutex), boost::ref(step)));
  sched->run();
}

void rw_mutex_write(reactor::RWMutex& mutex, int& step)
{
  reactor::Lock lock(*sched, mutex.write());
  ++step;
  int prev = step;
  yield();
  BOOST_CHECK_EQUAL(step, prev);
}

void test_rw_mutex_multi_write()
{
  Fixture f;
  reactor::RWMutex mutex;
  int step = 0;
  reactor::Thread r1(*sched, "writer1",
                     boost::bind(rw_mutex_write,
                                 boost::ref(mutex), boost::ref(step)));
  reactor::Thread r2(*sched, "writer2",
                     boost::bind(rw_mutex_write,
                                 boost::ref(mutex), boost::ref(step)));
  reactor::Thread r3(*sched, "writer3",
                     boost::bind(rw_mutex_write,
                                 boost::ref(mutex), boost::ref(step)));
  sched->run();
}

void rw_mutex_both_read(reactor::RWMutex& mutex, int& step)
{
  reactor::Lock lock(*sched, mutex);
  int v = step;
  BOOST_CHECK_EQUAL(v % 2, 0);
  BOOST_CHECK_EQUAL(step, v);
  yield();
  BOOST_CHECK_EQUAL(step, v);
  yield();
  BOOST_CHECK_EQUAL(step, v);
}

void rw_mutex_both_write(reactor::RWMutex& mutex, int& step)
{
  reactor::Lock lock(*sched, mutex.write());
  ++step;
  yield();
  yield();
  ++step;
  BOOST_CHECK_EQUAL(step % 2, 0);
}

void test_rw_mutex_both()
{
  Fixture f;
  reactor::RWMutex mutex;
  int step = 0;
  reactor::Thread r1(*sched, "reader1",
                     boost::bind(rw_mutex_both_read,
                                 boost::ref(mutex), boost::ref(step)));
  reactor::Thread r2(*sched, "reader2",
                     boost::bind(rw_mutex_both_read,
                                 boost::ref(mutex), boost::ref(step)));
  sched->step();


  reactor::Thread w1(*sched, "writer1",
                     boost::bind(rw_mutex_both_write,
                                 boost::ref(mutex), boost::ref(step)));

  reactor::Thread w2(*sched, "writer2",
                     boost::bind(rw_mutex_both_write,
                                 boost::ref(mutex), boost::ref(step)));
  while (!r1.done())
    sched->step();
  BOOST_CHECK(r2.done());
  sched->step();

  reactor::Thread r3(*sched, "reader3",
                     boost::bind(rw_mutex_both_read,
                                 boost::ref(mutex), boost::ref(step)));
  reactor::Thread r4(*sched, "reader4",
                     boost::bind(rw_mutex_both_read,
                                 boost::ref(mutex), boost::ref(step)));
  while (!w1.done() || !w2.done())
    sched->step();

  sched->step();


  reactor::Thread w3(*sched, "writer2",
                     boost::bind(rw_mutex_both_write,
                                 boost::ref(mutex), boost::ref(step)));

  reactor::Thread w4(*sched, "writer4",
                     boost::bind(rw_mutex_both_write,
                                 boost::ref(mutex), boost::ref(step)));

  sched->run();
}

/*--------.
| Storage |
`--------*/

void storage(reactor::LocalStorage<int>& val, int start)
{
  val.Get() = start;
  yield();
  BOOST_CHECK_EQUAL(val.Get(), start);
  val.Get()++;
  yield();
  BOOST_CHECK_EQUAL(val.Get(), start + 1);
}

void test_storage()
{
  Fixture f;
  reactor::LocalStorage<int> val;

  reactor::Thread t1(*sched, "1", boost::bind(storage, boost::ref(val), 0));
  reactor::Thread t2(*sched, "2", boost::bind(storage, boost::ref(val), 1));
  reactor::Thread t3(*sched, "3", boost::bind(storage, boost::ref(val), 2));
  reactor::Thread t4(*sched, "4", boost::bind(storage, boost::ref(val), 3));

  sched->run();
}

/*----------.
| Terminate |
`----------*/

void terminate_starting()
{
  Fixture f;

  reactor::Thread starting(*sched, "starting", &empty);
  starting.terminate_now();
}

/*-----.
| Main |
`-----*/

bool test_suite()
{
  boost::unit_test::test_suite* basics = BOOST_TEST_SUITE("Basics");
  boost::unit_test::framework::master_test_suite().add(basics);
  basics->add(BOOST_TEST_CASE(test_basics_one));
  basics->add(BOOST_TEST_CASE(test_basics_interleave));

  boost::unit_test::test_suite* signals = BOOST_TEST_SUITE("Signals");
  boost::unit_test::framework::master_test_suite().add(signals);
  signals->add(BOOST_TEST_CASE(test_signals_one_on_one));
  signals->add(BOOST_TEST_CASE(test_signals_one_on_two));
  signals->add(BOOST_TEST_CASE(test_signals_two_on_one));
  signals->add(BOOST_TEST_CASE(test_signals_timeout));

  boost::unit_test::test_suite* sleep = BOOST_TEST_SUITE("Sleep");
  boost::unit_test::framework::master_test_suite().add(sleep);
  sleep->add(BOOST_TEST_CASE(test_sleep_interleave));
  sleep->add(BOOST_TEST_CASE(test_sleep_timing));

  boost::unit_test::test_suite* join = BOOST_TEST_SUITE("Join");
  boost::unit_test::framework::master_test_suite().add(join);
  join->add(BOOST_TEST_CASE(test_join));
  join->add(BOOST_TEST_CASE(test_join_multiple));
  join->add(BOOST_TEST_CASE(test_join_timeout));

  boost::unit_test::test_suite* timeout = BOOST_TEST_SUITE("Timeout");
  boost::unit_test::framework::master_test_suite().add(timeout);
  timeout->add(BOOST_TEST_CASE(test_timeout_do));
  timeout->add(BOOST_TEST_CASE(test_timeout_dont));

  boost::unit_test::test_suite* vthread = BOOST_TEST_SUITE("Return value");
  boost::unit_test::framework::master_test_suite().add(vthread);
  vthread->add(BOOST_TEST_CASE(test_vthread));

  boost::unit_test::test_suite* mt = BOOST_TEST_SUITE("Multithreading");
  boost::unit_test::framework::master_test_suite().add(mt);
  mt->add(BOOST_TEST_CASE(test_multithread_spawn_wake));
  mt->add(BOOST_TEST_CASE(test_multithread_run));

  boost::unit_test::test_suite* sem = BOOST_TEST_SUITE("Semaphore");
  boost::unit_test::framework::master_test_suite().add(sem);
  sem->add(BOOST_TEST_CASE(test_semaphore_noblock));
  sem->add(BOOST_TEST_CASE(test_semaphore_block));
  sem->add(BOOST_TEST_CASE(test_semaphore_multi));

  boost::unit_test::test_suite* mtx = BOOST_TEST_SUITE("Mutex");
  boost::unit_test::framework::master_test_suite().add(mtx);
  mtx->add(BOOST_TEST_CASE(test_mutex));

  boost::unit_test::test_suite* rwmtx = BOOST_TEST_SUITE("RWMutex");
  boost::unit_test::framework::master_test_suite().add(rwmtx);
  rwmtx->add(BOOST_TEST_CASE(test_rw_mutex_multi_read));
  rwmtx->add(BOOST_TEST_CASE(test_rw_mutex_multi_write));
  rwmtx->add(BOOST_TEST_CASE(test_rw_mutex_both));

  boost::unit_test::test_suite* storage = BOOST_TEST_SUITE("Storage");
  boost::unit_test::framework::master_test_suite().add(storage);
  storage->add(BOOST_TEST_CASE(test_storage));

  return true;
}

int
main(int argc, char** argv)
{
  return ::boost::unit_test::unit_test_main(test_suite, argc, argv);
}
