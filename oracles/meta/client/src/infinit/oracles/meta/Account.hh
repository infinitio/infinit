#ifndef INFINIT_ORACLES_META_CLIENT_ACCOUNT_HH
# define INFINIT_ORACLES_META_CLIENT_ACCOUNT_HH

# include <elle/Printable.hh>

# include <das/model.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      struct Account
        : public elle::Printable
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Account() = default;
        Account(elle::serialization::SerializerIn& s);
        std::string id;
        std::string type;

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

DAS_MODEL_FIELD(infinit::oracles::meta::Account, id);
DAS_MODEL_FIELD(infinit::oracles::meta::Account, type);

#endif
