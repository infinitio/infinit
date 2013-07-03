#include "UploadOperation.hh"

#include <elle/log.hh>
#include <elle/Exception.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.UploadOperation");

namespace surface
{
  namespace gap
  {
    UploadOperation::UploadOperation(std::string const& transaction_id,
                                     NetworkManager& network_manager,
                                     TransactionManager& transaction_manager,
                                     InfinitInstanceManager& infinit_instance_manager,
                                     NotifyFunc notify):
      Operation{"notify_upload_" + transaction_id},
      _notify(notify),
      _network_manager(network_manager),
      _transaction_manager(transaction_manager),
      _infinit_instance_manager(infinit_instance_manager),
      _transaction_id(transaction_id)
    {
      ELLE_TRACE_METHOD("");
    }

    void
    UploadOperation::_run()
    {
      ELLE_DEBUG_METHOD("");

      auto tr = this->_transaction_manager.one(this->_transaction_id);
      auto network_id = tr.network_id;
      try
      {
        this->_network_manager.wait_portal(network_id);
        this->_notify();
        // XXX This awaits the transaction to be terminated
        //while (this->_infinit_instance_manager.exists(network_id))
        //{
        //  tr = this->_transaction_manager.one(this->_transaction_id);
        //  if (tr.status != plasma::TransactionStatus::started)
        //    return;
        //  if (this->cancelled())
        //    return;
        //  ::sleep(1);
        //}
        //if (!this->_infinit_instance_manager.exists(network_id))
        //{
        //  throw elle::Exception{"infinit instance does not exist anymore"};
        //}
      }
      catch (...)
      {
        ELLE_ERR("cannot connect infinit instances: %s",
                 elle::exception_string());
        throw;
      }
    }

    void
    UploadOperation::_on_error()
    {
      this->_transaction_manager.update(
        this->_transaction_id,
        plasma::TransactionStatus::failed);
    }
  }
}
