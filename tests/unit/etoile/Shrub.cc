#define BOOST_TEST_MODULE Shrub
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <reactor/scheduler.hh>

#include <etoile/shrub/Shrub.hh>

void
test()
{
  etoile::shrub::Shrub shrub(16,
                             boost::posix_time::seconds(10),
                             boost::posix_time::milliseconds(100));
}

BOOST_AUTO_TEST_CASE(test_shrub)
{
  reactor::Scheduler sched;
  reactor::Thread t(sched, "main", [&] { test(); });
  sched.run();
}
