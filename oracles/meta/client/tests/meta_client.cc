#include <elle/test.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/meta/Client.hh>

ELLE_TEST_SCHEDULED(connection_refused)
{
  infinit::oracles::meta::Client c("127.0.0.1", 21232);
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(connection_refused));
}
