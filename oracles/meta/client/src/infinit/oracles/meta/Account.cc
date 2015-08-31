#include <elle/serialization/json/TypeError.hh>

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
          case AccountPlanType::AccountPlanType_Plus:
            return "plus";
          case AccountPlanType::AccountPlanType_Premium:
            return "premium";
          case AccountPlanType::AccountPlanType_Team:
            return "team";
          // Fallback.
          default:
            return "plus";
        }
      }

      Account::Quotas::QuotaUsage::QuotaUsage()
        : quota()
        , used(0)
      {
      }

      Account::Quotas::QuotaUsage::QuotaUsage(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::Quotas::QuotaUsage::serialize(elle::serialization::Serializer& s)
      {
        // XXX: Remove the try catch when feature/json-reset-optionals is
        // merged.
        try
        {
          s.serialize("quota", this->quota);
        }
        catch (elle::serialization::TypeError const&)
        {
          this->quota.reset();
        }
        s.serialize("used", this->used);
      }

      void
      Account::Quotas::QuotaUsage::print(std::ostream& stream) const
      {
        stream << "Quota(" << this->used << "/" << this->quota << ")";
      }

      Account::Quotas::Limit::Limit(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::Quotas::Limit::serialize(elle::serialization::Serializer& s)
      {
        // XXX: Remove the try catch when feature/json-reset-optionals is
        // merged.
        try
        {
          s.serialize("limit", this->limit);
        }
        catch (elle::serialization::TypeError const&)
        {
          this->limit.reset();
        }
      }

      void
      Account::Quotas::Limit::print(std::ostream& stream) const
      {
        stream << "Limit(" << this->limit << ")";
      }

      Account::Quotas::Quotas(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::Quotas::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("send_to_self", this->send_to_self);
        s.serialize("links", this->links);
        s.serialize("p2p", this->p2p);
      }

      void
      Account::Quotas::print(std::ostream& stream) const
      {
        stream << "Quotas("
               << "send to self = " << this->send_to_self << ", "
               << "links = " << this->links << ", "
               << "p2p = " << this->p2p << ")";
      }

      Account::Account(Account const& rhs)
        : custom_domain(rhs.custom_domain)
        , link_format(rhs.link_format)
        , plan(rhs.plan)
        , quotas(rhs.quotas)
        , _changed()
      {}

      Account::Account(elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      Account&
      Account::operator =(Account const& rhs)
      {
        this->custom_domain = rhs.custom_domain;
        this->link_format = rhs.link_format;
        this->plan = rhs.plan;
        this->quotas = rhs.quotas;
        // Do not overwrite the changed signal.
        this->_changed(*this);
        return *this;
      }

      void
      Account::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("custom_domain", this->custom_domain);
        s.serialize("link_format", this->link_format);
        s.serialize("plan", this->plan);
        s.serialize("quotas", this->quotas);
      }

      void
      Account::print(std::ostream& stream) const
      {
        stream << "Account(" << account_type_string(this->plan) << ", "
               << "custom_domain = " << this->custom_domain << ", "
               << "quotas = " << this->quotas << ")";
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
      else if (repr == "plus")
        return AccountPlanType::AccountPlanType_Plus;
      else if (repr == "premium")
        return AccountPlanType::AccountPlanType_Premium;
      else if (repr == "team")
        return AccountPlanType::AccountPlanType_Team;
      // Fallback to Plus.
      return AccountPlanType::AccountPlanType_Plus;
    }
  }
}
