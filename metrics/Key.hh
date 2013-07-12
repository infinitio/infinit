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
    network,
    panel,
    session,
    size,
    status,
    step,
    tag,
    timestamp,
    value,
    width,
    sender_online,
    recipient_online,
  };

  std::ostream&
  operator <<(std::ostream& out,
              Key const k);
}

# include "Key.hxx"

#endif
