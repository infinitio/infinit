#include <infinit/oracles/meta/ExternalAccount.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      ExternalAccount::ExternalAccount(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      ExternalAccount::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("id", this->id);
        s.serialize("type", this->type);
      }

      void
      ExternalAccount::print(std::ostream& stream) const
      {
        stream << "ExternalAccount(" << this->type << ": " << this->id << ")";
      }
    }
  }
}
