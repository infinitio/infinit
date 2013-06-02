#include "UploadOperation.hh"

#include <elle/log.hh>
#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("surface.gap.UploadOperation");

namespace surface
{
  namespace gap
  {
    UploadOperation::UploadOperation(std::string const& transaction_id,
                                     NotifyFunc notify):
      Operation{"notify_upload_" + transaction_id},
      _notify(notify)
    {}

    void
    UploadOperation::_run()
    {
      try
      {
        this->_notify();
      }
      catch (...)
      {
        ELLE_ERR("cannot connect infinit instances: %s",
                 elle::exception_string());
        throw;
      }
    }
  }
}
