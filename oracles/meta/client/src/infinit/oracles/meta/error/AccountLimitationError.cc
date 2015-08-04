#include <infinit/oracles/meta/error/AccountLimitationError.hh>

#include <elle/printf.hh>

namespace infinit
{
  namespace oracles
  {
    namespace meta
    {
      AccountLimitationError::AccountLimitationError(MetaError const& error,
                                                     std::string const& reason)
        : elle::Error(reason)
        , _meta_error(error)
      {}

      LinkQuotaExceeded::LinkQuotaExceeded(MetaError const& error,
                                           uint64_t quota,
                                           uint64_t usage)
        : AccountLimitationError(
            error,
            elle::sprintf("Link storage limit reached (%s / %s).",
            usage, quota))
        , _quota(quota)
        , _usage(usage)
      {}

      SendToSelfTransactionLimitReached::SendToSelfTransactionLimitReached(
        infinit::oracles::meta::Error const& error)
          : AccountLimitationError(error,
                                   "Send to self transaction limit reached.")
      {}

      TransferSizeLimitExceeded::TransferSizeLimitExceeded(MetaError const& error)
        : AccountLimitationError(error, "File transfer size limited.")
      {}
    }
  }
}
