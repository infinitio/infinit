#include "TransactionStateMachine.hh"

#include <elle/assert.hh>
#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.surface.gap.TransactionStateMachine");

namespace surface
{
  namespace gap
  {
    TransactionStateMachine::TransactionStateMachine(
        CanceledCallback canceled,
        CleanCallback clean,
        PrepareUploadCallback prepare_upload,
        StartUploadCallback start_upload,
        StartDownloadCallback start_download,
        DeviceStatusCallback device_status,
        Self self,
        Device device):
      _canceled{canceled},
      _clean{clean},
      _prepare_upload{prepare_upload},
      _start_upload{start_upload},
      _start_download{start_download},
      _device_status{device_status},
      _self{self},
      _device(device) // XXX bug g++4.7 with braces
    {
      ELLE_ASSERT(this->_canceled != nullptr);
      ELLE_ASSERT(this->_clean != nullptr);
      ELLE_ASSERT(this->_prepare_upload != nullptr);
      ELLE_ASSERT(this->_start_upload != nullptr);
      ELLE_ASSERT(this->_device_status != nullptr);
      ELLE_ASSERT(this->_start_download != nullptr);
    }

    void
    TransactionStateMachine::operator ()(Transaction const& tr)
    {
      ELLE_TRACE_SCOPE("Evaluate %s", tr);
      if (tr.status == plasma::TransactionStatus::canceled)
      {
        ELLE_DEBUG("fire canceled callback")
          this->_canceled(tr);
      }
      if (tr.status != plasma::TransactionStatus::created and
               tr.status != plasma::TransactionStatus::started)
      {
        ELLE_DEBUG("cleaning up finished transaction")
          this->_clean(tr);
        return;
      }

      if (tr.sender_id == this->_self.id)
      {
        if (tr.sender_device_id != this->_device.id)
        {
          ELLE_ERR(
            "got a transaction %s that does not involve my device", tr);
          ELLE_ASSERT(false && "invalid transaction_id");
          return;
        }
        if (tr.status == plasma::TransactionStatus::created)
        {
          ELLE_DEBUG("sender prepare upload for %s", tr)
            this->_prepare_upload(tr);
        }
        else if (tr.status == plasma::TransactionStatus::started &&
                 tr.accepted &&
                 this->_device_status(tr.recipient_id,
                                      tr.recipient_device_id))
        {
          ELLE_DEBUG("sender start upload for %s", tr)
            this->_start_upload(tr);

        }
        else
        {
          ELLE_DEBUG("sender does nothing for %s", tr);
        }
      }
      else if (tr.recipient_id == this->_self.id)
      {
        if (tr.recipient_device_id != this->_device.id)
        {
          if (!tr.recipient_device_id.empty())
          {
            ELLE_ERR(
              "got an accepted transaction that does not involve my device: %s",
              tr);
            ELLE_ASSERT(false && "invalid transaction_id");
            return;
          }
        }
        if (tr.status == plasma::TransactionStatus::started &&
            tr.accepted &&
            this->_device_status(tr.sender_id,
                                 tr.sender_device_id))
        {
          ELLE_DEBUG("recipient start download for %s", tr)
            this->_start_download(tr);
        }
        else
        {
#ifdef DEBUG
          std::string reason;
          if (tr.status == plasma::TransactionStatus::created)
            reason = "transaction not started yet";
          else if (tr.status != plasma::TransactionStatus::started)
            reason = "transaction is terminated";
          else if (!tr.accepted)
            reason = "transaction not accepted yet";
          else if (!this->_device_status(tr.sender_id,
                                         tr.sender_device_id))
            reason = "sender device_id is down";
          ELLE_DEBUG("recipient does nothing for %s (%s)", tr, reason);
#endif
        }
      }
      else
      {
        ELLE_ERR("got a transaction not related to me:", tr);
        ELLE_ASSERT(false && "invalid transaction_id");
        return;
      }
    }
  }
}
