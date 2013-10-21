# include <infinit/oracles/meta/Transaction.hh>
#include <iostream>

namespace infinit
{
  namespace oracles
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
      message(),
      files(),
      files_count(),
      total_size(),
      is_directory(),
      status(Status::created),
      ctime(),
      mtime()
    {}

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
                Transaction::Status t)
    {
      switch (t)
      {
# define TRANSACTION_STATUS(name, value)                                       \
        case Transaction::Status::name:                                        \
          out << #name;                                                        \
          break;
# include <infinit/oracles/meta/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
      }

      return out;
    }

    std::ostream&
    operator <<(std::ostream& out,
                Transaction const& t)
    {
      out
        << "<Transaction(" << t.id
        << ", ctime=" << t.ctime
        << ", mtime=" << t.mtime
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
}
