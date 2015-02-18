#include "uuids.hh"

#include <boost/lexical_cast.hpp>

namespace std
{
  std::size_t
  hash<boost::uuids::uuid>::operator()(boost::uuids::uuid const& s) const
  {
    auto hasher = hash<string>{};
    return hasher(boost::lexical_cast<string>(s));
  }
}
