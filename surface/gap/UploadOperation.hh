#ifndef SURFACE_GAP_UPLOADOPERATION_HH
# define SURFACE_GAP_UPLOADOPERATION_HH

# include "OperationManager.hh"

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

    public:
      UploadOperation(std::string const& transaction_id,
                      NotifyFunc _notify_func);

      void
      _run() override;
    };
  }
}

#endif
