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
      metrics::Reporter& _reporter;
      plasma::meta::SelfResponse& _self;
      Transaction _transaction;
      std::unordered_set<std::string> _files;

    public:
      PrepareTransactionOperation(
          TransactionManager& transaction_manager,
          NetworkManager& network_manager,
          plasma::meta::Client& meta,
          metrics::Reporter& reporter,
          plasma::meta::SelfResponse& me,
          Transaction const& transaction,
          std::unordered_set<std::string> const& files);

    protected:
      void
      _run() override;

      void
      _cancel() override;

      void
      _on_error() override;
    };
  }
}

#endif
