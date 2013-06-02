#ifndef SURFACE_GAP_PREPARETRANSACTIONOPERATION_HH
# define SURFACE_GAP_PREPARETRANSACTIONOPERATION_HH

# include "OperationManager.hh"
# include "TransactionManager.hh"
# include "NetworkManager.hh"

# include <plasma/meta/Client.hh>

# include <elle/log.hh>

# include <string>
# include <unordered_set>

namespace surface
{
  namespace gap
  {
    class PrepareTransactionOperation:
      public Operation
    {
      TransactionManager& _transaction_manager;
      NetworkManager& _network_manager;
      plasma::meta::Client& _meta;
      elle::metrics::Reporter& _reporter;
      plasma::meta::SelfResponse& _me;
      Transaction _transaction;

    public:
      PrepareTransactionOperation(
          TransactionManager& transaction_manager,
          NetworkManager& network_manager,
          plasma::meta::Client& meta,
          elle::metrics::Reporter& reporter,
          plasma::meta::SelfResponse& me,
          Transaction const& transaction);

    protected:
      void
      _run() override;

      void
      _cancel() override;
    };
  }
}

#endif
#endif

