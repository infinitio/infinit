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
    connection_method,
    duration,
    file_count,
    file_size,
    height,
    how_ended,
    input,
    method,
    metric_from,
    network,
    panel,
    recipient,
    recipient_online,
    sender,
    sender_online,
    session,
    size,
    source,
    status,
    step,
    tag,
    transaction_id,
    timestamp,
    value,
    who,
    who_connected,
    who_ended,
    width,
  };

  std::ostream&
  operator <<(std::ostream& out,
              Key const k);
}

# include "Key.hxx"

#endif
