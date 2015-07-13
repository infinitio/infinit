#include <infinit/oracles/meta/Account.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      static
      std::string
      account_type_string(AccountPlanType account_type)
      {
        switch (account_type)
        {
          case AccountPlanType::AccountPlanType_Basic:
            return "basic";
          case AccountPlanType::AccountPlanType_Premium:
            return "premium";
          // Fallback.
          default:
            return "basic";
        }
      }

      Account::Account(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("custom_domain", this->custom_domain);
        s.serialize("link_format", this->link_format);
        s.serialize("plan", this->plan);
        s.serialize("link_size_quota", this->link_size_quota);
        s.serialize("link_size_used", this->link_size_used);
      }

      void
      Account::print(std::ostream& stream) const
      {
        stream << "Account(" << account_type_string(this->plan)
               << ", custom_domain = " << this->custom_domain
               << ", link_size_used = " << this->link_size_used
               << ", link_size_quota = " << this->link_size_quota << ")";
      }

      std::ostream&
      operator <<(std::ostream& output, AccountPlanType account_type)
      {
        output << account_type_string(account_type);
        return output;
      }
    }
  }
}

namespace elle
{
  namespace serialization
  {
    using infinit::oracles::meta::AccountPlanType;

    std::string
    Serialize<AccountPlanType>::convert(AccountPlanType& account_type)
    {
      return infinit::oracles::meta::account_type_string(account_type);
    }

    AccountPlanType
    Serialize<AccountPlanType>::convert(std::string& repr)
    {
      if (repr == "basic")
        return AccountPlanType::AccountPlanType_Basic;
      else if (repr == "premium")
        return AccountPlanType::AccountPlanType_Premium;
      // Fallback.
      return AccountPlanType::AccountPlanType_Basic;
    }
  }
}
