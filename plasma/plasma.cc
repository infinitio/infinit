#include "plasma.hh"

#include <iostream>

namespace plasma
{
  Transaction::Transaction(std::string const& sender_id,
                           std::string const& sender_fullname,
                           std::string const& sender_device_id):
    id(),
    sender_id(sender_id),
    sender_fullname(sender_fullname),
    sender_device_id(sender_device_id),
    recipient_id(),
    recipient_fullname(),
    recipient_device_id(),
    recipient_device_name(),
    network_id(),
    message(),
    files(),
    files_count(),
    total_size(),
    is_directory(),
    status(TransactionStatus::created),
    timestamp()
  {
  }

  bool
  Transaction::empty() const
  {
    return this->id.empty();
  }

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
      << ", files=" << elle::sprint(t.files)
      << ", status=" << t.status
      << ") "
      << "from " << t.sender_fullname << " (" << t.sender_id << ") "
      << "on device " << t.sender_device_id << ", "
      << "to " << t.recipient_fullname << " (" << t.recipient_id << ") "
      << "on device " << t.recipient_device_id << '>';

    return out;
  }
}
