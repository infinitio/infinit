#include <infinit/oracles/meta/AddressBookContact.hh>

# include <elle/serialization/json.hh>
# include <elle/serialization/Serializer.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      AddressBookContact::AddressBookContact(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      AddressBookContact::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("emails", this->email_addresses);
        s.serialize("phones", this->phone_numbers);
      }

      void
      AddressBookContact::print(std::ostream& stream) const
      {
        stream << "AddressBookContact(emails: " << this->email_addresses.size()
               << ", phone numbers: " << this->phone_numbers.size() << ")";
      }
    }
  }
}
