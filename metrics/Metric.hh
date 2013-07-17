#ifndef METRICS_METRIC_HH
# define METRICS_METRIC_HH

# include "Key.hh"

# include <elle/utility/Time.hh>
# include <elle/container/map.hh>

# include <string>
# include <unordered_map>

namespace metrics
{
  typedef std::unordered_map<Key, std::string> Metric;
  typedef std::pair<elle::utility::Time, Metric> TimeMetricPair;
}

#endif
