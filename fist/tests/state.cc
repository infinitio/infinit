#include <elle/log.hh>
#include <elle/test.hh>

#include <reactor/scheduler.hh>

#include <surface/gap/State.hh>

ELLE_LOG_COMPONENT("surface.gap.State.test");

ELLE_TEST_SCHEDULED(forbidden_trophonius)
{

}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(forbidden_trophonius));
}
