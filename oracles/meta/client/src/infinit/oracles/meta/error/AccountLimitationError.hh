#ifndef INFINIT_ORACLES_META_ACCOUNTLIMITATIONERROR_HH
# define INFINIT_ORACLES_META_ACCOUNTLIMITATIONERROR_HH

# include <elle/Error.hh>

# include <infinit/oracles/meta/Error.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      class AccountLimitationError
        : public elle::Error
      {
      public:
        typedef infinit::oracles::meta::Error MetaError;
        AccountLimitationError(MetaError const& error,
                               std::string const& reason = "");

        ELLE_ATTRIBUTE_R(MetaError, meta_error);
      };

      class LinkQuotaExceeded
        : public AccountLimitationError
      {
      public:
        LinkQuotaExceeded(MetaError const& error,
                          uint64_t quota,
                          uint64_t usage);

        ELLE_ATTRIBUTE_R(uint64_t, quota);
        ELLE_ATTRIBUTE_R(uint64_t, usage);
      };

      class SendToSelfTransactionLimitReached
        : public AccountLimitationError
      {
      public:
        SendToSelfTransactionLimitReached(MetaError const& error);
      };

      class TransferSizeLimitExceeded
        : public AccountLimitationError
      {
      public:
        TransferSizeLimitExceeded(MetaError const& error);
      };
    }
  }
}

#endif
