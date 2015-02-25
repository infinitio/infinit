#include <elle/log.hh>

#include "uuids.hh"

#include <boost/lexical_cast.hpp>

ELLE_LOG_COMPONENT("fist.tests.uuids");

boost::uuids::uuid
random_uuid()
{
  typedef boost::uuids::basic_random_generator<boost::mt19937> Generator;
  Generator generator{};
  auto res = generator();
  ELLE_LOG("generated %s", res);
  return res;
}

namespace std
{
  std::size_t
  hash<boost::uuids::uuid>::operator()(boost::uuids::uuid const& s) const
  {
    auto hasher = hash<string>{};
    return hasher(boost::lexical_cast<string>(s));
  }
}
