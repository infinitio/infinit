#ifndef INFINIT_ORACLES_META_CLIENT_EXTERNAL_ACCOUNT_HH
# define INFINIT_ORACLES_META_CLIENT_EXTERNAL_ACCOUNT_HH

# include <elle/Printable.hh>

# include <das/model.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      struct ExternalAccount
        : public elle::Printable
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        ExternalAccount() = default;
        ExternalAccount(elle::serialization::SerializerIn& s);
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

DAS_MODEL_FIELD(infinit::oracles::meta::ExternalAccount, id);
DAS_MODEL_FIELD(infinit::oracles::meta::ExternalAccount, type);

#endif
