#include <boost/filesystem.hpp>

#include <elle/log.hh>
#include <elle/Exception.hh>

#include <reactor/network/exception.hh>
#include <reactor/scheduler.hh>

#include <surface/gap/onboarding/TransferMachine.hh>

ELLE_LOG_COMPONENT("surface.gap.onboarding.TransferMachine");

namespace surface
{
  namespace gap
  {
    namespace onboarding
    {
      TransferMachine::TransferMachine(
        TransactionMachine& owner,
        std::string const& file_path,
        std::string const& output_dir,
        reactor::Duration duration):
          Transferer(owner),
          _progress(0.0f),
          _duration(duration),
          _running("running"),
          _interrupt(false),
          _file_path(file_path),
          _output_dir(output_dir)
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
      }

      void
      TransferMachine::_cloud_buffer()
      {
        // Show fake cloud buffering.
      }

      void
      TransferMachine::_cloud_synchronize()
      {
        // Show fake cloud synchronizing.
      }

      void
      TransferMachine::_transfer()
      {
        ELLE_TRACE_SCOPE("%s: running transfer", *this);
        while (this->_progress < 1.0f)
        {
          static reactor::Duration step = 100_ms;
          this->_running.wait();
          if (this->_interrupt)
          {
            this->_interrupt = false;
            throw reactor::network::Exception("transfer interrupted by user");
          }
          this->_progress +=
            (step.total_milliseconds() /
             (1.0f * this->_duration.total_milliseconds()));
          reactor::sleep(step);
        }
        if (this->_progress > 1.0f)
          this->_progress = 1.0f;
        try
        {
          boost::filesystem::path input_path(this->_file_path);
          boost::filesystem::path output_dir(this->_output_dir);
          auto output_path = output_dir / input_path.filename();
          boost::filesystem::copy_file(input_path, output_path,
            boost::filesystem::copy_option::overwrite_if_exists);
        }
        catch (boost::filesystem::filesystem_error const& e)
        {
          ELLE_WARN("%s: error copying file: %s", *this, e.what());
          throw elle::Error("error while copying onboarding file");
        }
      }

      void
      TransferMachine::_stopped()
      {
      }

      void TransferMachine::_initialize()
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

      void
      TransferMachine::interrupt()
      {
        this->_interrupt = true;
      }
    }
  }
}
