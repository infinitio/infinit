#ifndef METRICS_KEY_HXX
# define METRICS_KEY_HXX

# include "Key.hh"

# include <functional> // std::hash

namespace std
{
  // Allow Key enum to be stored in a map.
  template <>
  struct hash<metrics::Key>
  {
    size_t
    operator ()(metrics::Key const k) const
    {
      return static_cast<size_t>(k);
    }
  };
}

#endif
