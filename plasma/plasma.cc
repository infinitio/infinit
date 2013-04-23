#include "plasma.hh"

#include <iostream>

namespace plasma
{
  std::ostream&
  operator <<(std::ostream& out,
              plasma::TransactionStatus t)
  {
    switch (t)
    {
# define TRANSACTION_STATUS(name, value)        \
      case plasma::TransactionStatus::name:     \
        out << #name;                           \
        break;
# include <oracle/disciples/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
    }

    return out;
  }

  std::ostream&
  operator <<(std::ostream& out,
              plasma::Transaction const& t)
  {
    out << "[tr: " << t.transaction_id << " - net: " << t.network_id << "] s: "
        << t.sender_id << " ( d:" << t.sender_device_id << ") "
        << t.first_filename << " -> r:" << t.recipient_id << " (d: "
        << t.recipient_device_id << ") : "
        << (plasma::TransactionStatus) t.status;

    return out;
  }
}
