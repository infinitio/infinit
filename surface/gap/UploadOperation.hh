#ifndef SURFACE_GAP_UPLOADOPERATION_HH
# define SURFACE_GAP_UPLOADOPERATION_HH

# include "OperationManager.hh"
# include "NetworkManager.hh"

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
      ELLE_ATTRIBUTE(plasma::Transaction, transaction);

    public:
      UploadOperation(plasma::Transaction const& transaction,
                      NetworkManager& network_manager,
                      NotifyFunc _notify_func);

      void
      _run() override;
    };
  }
}

#endif
