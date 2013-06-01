#ifndef SURFACE_GAP_UPLOADOPERATION_HH
# define SURFACE_GAP_UPLOADOPERATION_HH

# include "OperationManager.hh"

# include <string>
# include <functional>

namespace surface
{
  namespace gap
  {
    struct UploadOperation:
      public Operation
    {
      typedef std::function<void()> Notify8infinitFunc;
    private::
      Notify8infinitFunc _notify_func;

    public:
      UploadOperation(Notify8infinitFunc _notify_func);

      void
      _run() override;
    };
  }
}

#endif
