#ifndef FIST_SURFACE_GAP_TESTS_UUIDS_HH
# define FIST_SURFACE_GAP_TESTS_UUIDS_HH

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>

namespace std
{
  template <>
  struct hash<boost::uuids::uuid>
  {
  public:
    std::size_t operator()(boost::uuids::uuid const& s) const;
  };
}

#endif
