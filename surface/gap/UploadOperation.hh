#ifndef SURFACE_GAP_UPLOADOPERATION_HH
# define SURFACE_GAP_UPLOADOPERATION_HH

# include "OperationManager.hh"
# include "NetworkManager.hh"
# include "TransactionManager.hh"

# include <plasma/plasma.hh>

# include <elle/attribute.hh>

# include <string>
# include <functional>

namespace surface
{
  namespace gap
  {
    struct UploadOperation:
      public Operation
    {
      typedef std::function<void()> NotifyFunc;
    private:
      ELLE_ATTRIBUTE(NotifyFunc, notify);
      ELLE_ATTRIBUTE(NetworkManager&, network_manager);
      ELLE_ATTRIBUTE(TransactionManager&, transaction_manager);
      ELLE_ATTRIBUTE(InfinitInstanceManager&, infinit_instance_manager);
      ELLE_ATTRIBUTE(std::string const, transaction_id);

    public:
      UploadOperation(std::string const& transaction_id,
                      NetworkManager& network_manager,
                      TransactionManager& transaction_manager,
                      InfinitInstanceManager& infinit_instance_manager,
                      NotifyFunc _notify_func);

      void
      _run() override;
    };
  }
}

#endif
