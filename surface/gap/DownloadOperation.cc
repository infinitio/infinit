#include "DownloadOperation.hh"

#include <common/common.hh>

#include <elle/log.hh>
#include <elle/system/Process.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>

#include <boost/algorithm/string/join.hpp>

#include <list>
#include <string>

ELLE_LOG_COMPONENT("surface.gap.DownloadOperation");

namespace surface
{
  namespace gap
  {
    DownloadOperation::DownloadOperation(
        TransactionManager& transaction_manager,
        NetworkManager& network_manager,
        plasma::meta::SelfResponse const& me,
        plasma::Transaction const& transaction,
        std::function<void()> notify):
      Operation{"download_files_for_" + transaction.id},
      _transaction_manager(transaction_manager),
      _network_manager(network_manager),
      _me(me),
      _transaction(transaction),
      _notify{notify}
    {}

    void
    DownloadOperation::_run()
    {
      this->_network_manager.add_device(
        this->_transaction.network_id,
        this->_transaction.recipient_device_id);
      this->_notify();

      std::string const& transfer_binary =
        common::infinit::binary_path("8transfer");

      std::list<std::string> arguments{
        "-n", this->_transaction.network_id,
        "-u", this->_me.id,
        "--path", this->_transaction_manager.output_dir(),
        "--from"
      };

      ELLE_DEBUG("LAUNCH: %s %s",
                 transfer_binary,
                 boost::algorithm::join(arguments, " "));

      try
      {
        auto pc = elle::system::process_config(elle::system::normal_config);
        {
          std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

          if (!log_file.empty())
          {
            if (elle::os::in_env("INFINIT_LOG_FILE_PID"))
            {
              log_file += ".";
              log_file += std::to_string(::getpid());
            }
            log_file += ".from.transfer.log";
            pc.setenv("ELLE_LOG_FILE", log_file);
          }
        }

        {
          elle::system::Process p{std::move(pc), transfer_binary, arguments};
          ELLE_DEBUG("Waiting transfer --from process to finish");
          while (p.running())
          {
            if (p.wait_status(elle::system::Process::Milliseconds(10)) != 0)
              throw surface::gap::Exception(
                gap_internal_error,
                elle::sprintf("8transfer binary failed for network %s",
                              this->_transaction.network_id));

            if (this->cancelled())
            {
              ELLE_DEBUG("operation cancelled");
              // Terminate and wait.
              p.interrupt(elle::system::ProcessTermination::dont_wait);
              p.wait(elle::system::Process::Milliseconds(1000));
              return;
            }
          }
        }

        if (this->_transaction.files_count == 1)
        {
          ELLE_LOG("Download complete. Your file is at '%s'.",
                   elle::os::path::join(
                     this->_transaction_manager.output_dir(),
                     this->_transaction.first_filename));
        }
        else
        {
          ELLE_LOG("Download complete. Your %d files are in '%s'.",
                   this->_transaction.files_count,
                   this->_transaction_manager.output_dir());
        }

        this->_transaction_manager.update(this->_transaction.id,
                                          plasma::TransactionStatus::finished);
      }
      catch (...)
      {
        ELLE_ERR("couldn't receive file %s: %s",
                 this->_transaction.first_filename,
                 elle::exception_string());
        throw;
      }
    }

    void
    DownloadOperation::_cancel()
    {
      ELLE_DEBUG("cancelling %s name", this->name());
      this->_transaction_manager.update(
        this->_transaction.id,
        plasma::TransactionStatus::canceled);
    }
  }
}
