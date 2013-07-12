#ifndef SURFACE_GAP_DOWNLOADOPERATION_HH
# define SURFACE_GAP_DOWNLOADOPERATION_HH

# include "OperationManager.hh"
# include "TransactionManager.hh"

# include <plasma/plasma.hh>
# include <plasma/meta/Client.hh>

# include <functional>
# include <list>
# include <string>

namespace surface
{
  namespace gap
  {
    class DownloadOperation:
      public Operation
    {
      TransactionManager& _transaction_manager;
      NetworkManager& _network_manager;
      plasma::meta::SelfResponse const& _me;
      metrics::Reporter& _reporter;
      plasma::Transaction _transaction;
      std::function<void()> _notify;

    public:
      DownloadOperation(TransactionManager& transaction_manager,
                        NetworkManager& network_manager,
                        plasma::meta::SelfResponse const& me,
                        metrics::Reporter& reporter,
                        plasma::Transaction const& transaction,
                        std::function<void()> notify);

    protected:
      void
      _run() override;

      virtual
      void
      _on_error();

      virtual
      void
      _on_success();
    };
  }
}

#endif
