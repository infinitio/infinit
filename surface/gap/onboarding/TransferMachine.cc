#include <elle/log.hh>

#include <surface/gap/onboarding/TransferMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.onboarding.TransferMachine");

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      TransferMachine::TransferMachine(TransactionMachine& owner,
                                       reactor::Duration duration)
        : Transferer(owner)
        , _progress(0.0f)
        , _duration(duration)
        , _running("running")
      {
        ELLE_TRACE_SCOPE("%s: creation", *this);
        this->_running.open();
      }

      void
      TransferMachine::_publish_interfaces()
      {
        reactor::sleep(500_ms);
      }

      void
      TransferMachine::_connection()
      {
        reactor::sleep(1_sec);
      }

      void
      TransferMachine::_wait_for_peer()
      {
        // Show fake cloud buffering.
      }

      void
      TransferMachine::_transfer()
      {
        ELLE_TRACE_SCOPE("%s: running transfer", *this);
        while (this->_progress < 1.0f)
        {
          static reactor::Duration step = 200_ms;
          this->_running.wait();
          this->_progress +=
            (step.total_milliseconds() /
             (1.0f * this->_duration.total_milliseconds()));
          reactor::sleep(step);
        }
      }

      void
      TransferMachine::_stopped()
      {
      }

      float
      TransferMachine::progress() const
      {
        return this->_progress;
      }

      bool
      TransferMachine::finished() const
      {
        return (this->_progress >= 1.0f);
      }
    }
  }
}
