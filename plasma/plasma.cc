#include "plasma.hh"

#include <iostream>

namespace plasma
{
  Transaction::Transaction()
  {}

  Transaction::~Transaction()
  {}

  void
  Transaction::print(std::ostream& stream) const
  {
    stream << *this;
  }

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
# include <oracle/disciples/meta/src/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
    }

    return out;
  }

  std::ostream&
  operator <<(std::ostream& out,
              plasma::Transaction const& t)
  {
    out
      << "<Transaction(" << t.id
      << ", net=" << t.network_id
      << ", t=" << t.timestamp
      << ", file=" << t.first_filename
      << ", status=" << (plasma::TransactionStatus) t.status
      << ") "
      << "from " << t.sender_fullname << " (" << t.sender_id << ") "
      << "on device " << t.sender_device_id << ", "
      << "to " << t.recipient_fullname << " (" << t.recipient_id << ") "
      << "on device " << t.recipient_device_id << '>';

    return out;
  }
}
