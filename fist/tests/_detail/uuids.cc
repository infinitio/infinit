#include <elle/log.hh>

#include "uuids.hh"

#include <boost/lexical_cast.hpp>

ELLE_LOG_COMPONENT("fist.tests.uuids");

boost::uuids::uuid
random_uuid()
{
  boost::mt19937 ran;
  auto res = boost::uuids::random_generator(ran)();
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
