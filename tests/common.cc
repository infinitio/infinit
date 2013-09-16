#define BOOST_TEST_MODULE common
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <common/common.hh>
#include <boost/filesystem.hpp>

BOOST_AUTO_TEST_CASE(home_dir)
{
  std::string path = common::system::home_directory();
  BOOST_CHECK(boost::filesystem::exists(path));
}
