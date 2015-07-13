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
      typedef enum
        : unsigned int
      {
        AccountPlanType_Basic = 0,
        AccountPlanType_Premium,
      } AccountPlanType;

      struct Account
        : public elle::Printable
      {
      /*-------------.
      | Construction |
      `-------------*/
      public:
        Account() = default;
        Account(elle::serialization::SerializerIn& s);
        das::Variable<std::string> custom_domain;
        das::Variable<std::string> link_format;
        das::Variable<uint64_t> link_size_quota;
        das::Variable<uint64_t> link_size_used;
        das::Variable<AccountPlanType> plan;

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

      std::ostream&
      operator <<(std::ostream& output, AccountPlanType account_type);
    }
  }
}

# include <infinit/oracles/meta/Account.hxx>

DAS_MODEL(infinit::oracles::meta::Account,
          (custom_domain, link_format, plan, link_size_quota, link_size_used),
          DasAccount);
DAS_MODEL_DEFAULT(infinit::oracles::meta::Account, DasAccount);

#endif
