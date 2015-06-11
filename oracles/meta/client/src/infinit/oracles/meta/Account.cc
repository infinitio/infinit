#include <infinit/oracles/meta/Account.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      Account::Account(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("id", this->id);
        s.serialize("type", this->type);
      }

      void
      Account::print(std::ostream& stream) const
      {
        stream << "Account(" << this->type << ": " << this->id << ")";
      }
    }
  }
}
