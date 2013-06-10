#include "UploadOperation.hh"

#include <elle/log.hh>
#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("surface.gap.UploadOperation");

namespace surface
{
  namespace gap
  {
    UploadOperation::UploadOperation(plasma::Transaction const& transaction,
                                     NetworkManager& network_manager,
                                     NotifyFunc notify):
      Operation{"notify_upload_" + transaction.id},
      _notify(notify),
      _network_manager(network_manager),
      _transaction(transaction)
    {}

    void
    UploadOperation::_run()
    {
      try
      {
        this->_network_manager.wait_portal(this->_transaction.network_id);
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
