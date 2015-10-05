#ifndef INFINIT_ORACLES_META_CLIENT_ACCOUNT_HXX
# define INFINIT_ORACLES_META_CLIENT_ACCOUNT_HXX

namespace elle
{
  namespace serialization
  {
    template <>
    struct Serialize<infinit::oracles::meta::AccountPlanType>
    {
      typedef std::string Type;

      static
      std::string
      convert(infinit::oracles::meta::AccountPlanType& account_type);

      static
      infinit::oracles::meta::AccountPlanType
      convert(std::string& repr);
    };

    typedef infinit::oracles::meta::Account::ReferralActions::Referral::ReferralMethod ReferralMethod;
    template <>
    struct Serialize<ReferralMethod>
    {
      typedef std::string Type;

      static
      std::string
      convert(ReferralMethod& referral_method);

      static
      ReferralMethod
      convert(std::string& repr);
    };

    typedef infinit::oracles::meta::Account::ReferralActions::Referral::ReferralStatus ReferralStatus;
    template <>
    struct Serialize<ReferralStatus>
    {
      typedef std::string Type;

      static
      std::string
      convert(ReferralStatus& referral_status);

      static
      ReferralStatus
      convert(std::string& repr);
    };
  }
}

#endif
