#include "plasma.hh"

#include <iostream>

namespace plasma
{
  Transaction::Transaction()
  {}

  Transaction::~Transaction()
  {}

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
    out
      << "{"
      << "tr: " << t.id << ", net: " << t.network_id << ", "
      << "s: {id: " << t.sender_id << ", dev: " << t.sender_device_id << "}, "
      << "f: " << t.first_filename << ", "
      << "r: {id: " << t.recipient_id << ", dev: " << t.recipient_device_id << "},"
      << " st: " << (plasma::TransactionStatus) t.status
      << "}";

    return out;
  }
}
