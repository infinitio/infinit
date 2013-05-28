#ifndef SURFACE_GAP_DOWNLOADOPERATION_HH
# define SURFACE_GAP_DOWNLOADOPERATION_HH

# include "OperationManager.hh"
# include "TransactionManager.hh"

# include <plasma/plasma.hh>
# include <plasma/meta/Client.hh>

# include <list>
# include <string>

namespace surface
{
  namespace gap
  {
    class DownloadOperation:
      public OperationManager::Operation
    {
      TransactionManager& _transaction_manager;
      plasma::meta::SelfResponse const& _me;
      plasma::Transaction const& _transaction;

    public:
      DownloadOperation(TransactionManager& transaction_manager,
                        plasma::meta::SelfResponse const& me,
                        plasma::Transaction const& transaction);

    protected:
      void
      _run() override;

      void
      _cancel() override;
    };
  }
}

#endif
