#ifndef INFINIT_ORACLES_META_CLIENT_ADDRESS_BOOK_CONTACT_HH
# define INFINIT_ORACLES_META_CLIENT_ADDRESS_BOOK_CONTACT_HH

# include <string>
# include <vector>

# include <boost/optional.hpp>

# include <elle/Printable.hh>
# include <elle/serialization/fwd.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      struct AddressBookContact
        : public elle::Printable
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        AddressBookContact() = default;
        AddressBookContact(elle::serialization::SerializerIn& s);
        std::vector<std::string> email_addresses;
        std::vector<std::string> phone_numbers;

      /*--------------.
      | Serialization |
      `--------------*/
      public:
        void
        serialize(elle::serialization::Serializer& s);

      /*----------.
      | Printable |
      `----------*/
      protected:
        virtual
        void
        print(std::ostream& stream) const override;
      };
    }
  }
}

#endif