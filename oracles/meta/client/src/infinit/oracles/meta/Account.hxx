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
  }
}

#endif
