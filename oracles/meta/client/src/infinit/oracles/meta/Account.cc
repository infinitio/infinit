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

      static
      std::string
      referral_method_string(
        Account::ReferralActions::Referral::ReferralMethod referral_method)
      {
        typedef Account::ReferralActions::Referral::ReferralMethod ReferralMethod;
        switch (referral_method)
        {
          case ReferralMethod::ReferralMethod_Ghost:
            return "ghost";
          case ReferralMethod::ReferralMethod_Plain:
            return "plain";
          case ReferralMethod::ReferralMethod_Link:
            return "link";

          default:
            return "unknown";
        }
      }

      static
      std::string
      referral_status_string(
        Account::ReferralActions::Referral::ReferralStatus referral_status)
      {
        typedef Account::ReferralActions::Referral::ReferralStatus ReferralStatus;
        switch (referral_status)
        {
          case ReferralStatus::ReferralStatus_Pending:
            return "pending";
          case ReferralStatus::ReferralStatus_Complete:
            return "complete";
          case ReferralStatus::ReferralStatus_Blocked:
            return "blocked";

          default:
            return "unknown";
        }
      }

      Account::ReferralActions::Referral::Referral()
        : identifier()
        , method(ReferralMethod_Ghost)
        , status(ReferralStatus_Pending)
      {}

      Account::ReferralActions::Referral::Referral(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::ReferralActions::Referral::serialize(
        elle::serialization::Serializer& s)
      {
        s.serialize("has_logged_in", this->has_logged_in);
        s.serialize("identifier", this->identifier);
        s.serialize("method", this->method);
        s.serialize("status", this->status);
      }

      void
      Account::ReferralActions::Referral::print(std::ostream& stream) const
      {
        stream << "Referral(" << this->identifier << ", "
               << referral_method_string(this->method) << ", "
               << referral_status_string(this->status) << ", "
               << (this->has_logged_in ? "has logged in" : "hasn't logged in")
               << ")";
      }

      Account::ReferralActions::ReferralActions()
        : has_avatar()
        , facebook_posts()
        , twitter_posts()
        , referrals()
      {}

      Account::ReferralActions::ReferralActions(
        elle::serialization::SerializerIn& s)
      {
        this->serialize(s);
      }

      void
      Account::ReferralActions::serialize(elle::serialization::Serializer& s)
      {
        s.serialize("has_avatar", this->has_avatar);
        s.serialize("facebook_posts", this->facebook_posts);
        s.serialize("twitter_posts", this->twitter_posts);
        s.serialize("referrals", this->referrals);
      }

      void
      Account::ReferralActions::print(std::ostream& stream) const
      {
        stream << "ReferralActions(has avatar: "
               << (this->has_avatar ? "yes" : "no") << ", "
               << "facebook posts: " << this->facebook_posts << ", "
               << "twitter posts: " << this->twitter_posts << ", "
               << "referrals: " << this->referrals << ")";
      }

      Account::Quotas::QuotaUsage::QuotaUsage()
        : quota()
        , used(0)
      {}

      Account::Quotas::QuotaUsage::QuotaUsage(
        elle::serialization::SerializerIn& s)
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
        , referral_actions(rhs.referral_actions)
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
        this->referral_actions = rhs.referral_actions;
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
        s.serialize("referral_actions", this->referral_actions);
      }

      void
      Account::print(std::ostream& stream) const
      {
        stream << "Account(" << account_type_string(this->plan) << ", "
               << "custom_domain = " << this->custom_domain << ", "
               << "quotas = " << this->quotas << ", "
               << "referral actions: " << this->referral_actions << ")";
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

    std::string
    Serialize<ReferralMethod>::convert(ReferralMethod& referral_method)
    {
      return infinit::oracles::meta::referral_method_string(referral_method);
    }

    ReferralMethod
    Serialize<ReferralMethod>::convert(std::string& repr)
    {
      if (repr == "ghost_invite")
        return ReferralMethod::ReferralMethod_Ghost;
      else if (repr == "plain_invite")
        return ReferralMethod::ReferralMethod_Plain;
      else if (repr == "public_link")
        return ReferralMethod::ReferralMethod_Link;
      return ReferralMethod::ReferralMethod_Ghost;
    }

    std::string
    Serialize<ReferralStatus>::convert(ReferralStatus& referral_status)
    {
      return infinit::oracles::meta::referral_status_string(referral_status);
    }

    ReferralStatus
    Serialize<ReferralStatus>::convert(std::string& repr)
    {
      if (repr == "pending")
        return ReferralStatus::ReferralStatus_Pending;
      else if (repr == "complete")
        return ReferralStatus::ReferralStatus_Complete;
      else if (repr == "blocked")
        return ReferralStatus::ReferralStatus_Blocked;
      return ReferralStatus::ReferralStatus_Pending;
    }
  }
}
