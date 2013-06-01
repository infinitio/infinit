#include "UploadOperation.hh"

#include <elle/log.hh>

ELLE_LOG_COMPONENT("surface.gap.UploadOperation");

namespace surface
{
  namespace gap
  {
    UploadOperation::UploadOperation(Notify8infinitFunc _notify_func):
      Operation{"notify_8infinit_" + transaction.id},
      _transaction_manager(transaction_manager),
      _transaction(transaction),
      _device(device),
      notify_func(notify_8infinit)
    {}

    void
    UploadOperation::_run()
    {
      try
      {
        this->_notify_func();
      }
      catch (...)
      {
        ELLE_ERR("cannot connect infinit instances: %s",
                 elle::exception_string(exception));
        if (this->cancelled())
          return;
        throw;
      }
    }
  }
}
