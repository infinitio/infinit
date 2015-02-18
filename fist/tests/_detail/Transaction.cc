#include "Transaction.hh"

#include <elle/serialization/json.hh>

#include <fist/tests/_detail/uuids.hh>

namespace tests
{
  Transaction::Transaction()
  {
    this->id = boost::lexical_cast<std::string>(
      boost::uuids::random_generator()());
  }

  void
  Transaction::print(std::ostream& out) const
  {
    out << "Transaction("
        << this->id << ", "
        << this->status
        << ")";
  }

  std::string
  Transaction::json()
  {
    std::stringstream notification_stream;
    {
      typename elle::serialization::json::SerializerOut output(notification_stream);
      this->serialize(output);
    }
    return notification_stream.str();
  }

  std::string const&
  Transaction::id_getter() const
  {
    return this->id;
  }
}
