#ifndef METRICS_KEY_HH
# define METRICS_KEY_HH

# include <iosfwd>

namespace metrics
{
  enum class Key
  {
    attempt,
    author,
    count,
    duration,
    height,
    input,
    method,
    network,
    panel,
    recipient_online,
    sender_online,
    session,
    size,
    source,
    status,
    step,
    tag,
    timestamp,
    value,
    width,
  };

  std::ostream&
  operator <<(std::ostream& out,
              Key const k);
}

# include "Key.hxx"

#endif
