#define BOOST_TEST_MODULE gap

#include <boost/test/unit_test.hpp>

#include <elle/print.hh>

#include <surface/gap/gap.h>

BOOST_AUTO_TEST_CASE(dir)
{
  gap_Status st = gap_gather_crash_reports("foo", "bar");

  BOOST_CHECK_EQUAL(st, gap_Status::gap_file_not_found);
}

