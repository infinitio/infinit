#ifndef METRICS_KIND_HH
# define METRICS_KIND_HH

# include <iosfwd>

namespace metrics
{
  enum class Kind
  {
    all,
    user,
    network,
    transaction
  };

  std::ostream&
  operator <<(std::ostream& out,
              Kind const k);
}

#endif
