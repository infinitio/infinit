#include "DownloadOperation.hh"

#include "binary_config.hh"

#include <common/common.hh>

#include <elle/log.hh>
#include <elle/system/Process.hh>
#include <elle/os/getenv.hh>
#include <elle/os/path.hh>

#include <boost/algorithm/string/join.hpp>

#include <chrono>
#include <list>
#include <string>

ELLE_LOG_COMPONENT("infinit.surface.gap.DownloadOperation");

namespace surface
{
  namespace gap
  {
    DownloadOperation::DownloadOperation(
        TransactionManager& transaction_manager,
        NetworkManager& network_manager,
        plasma::meta::SelfResponse const& me,
        elle::metrics::Reporter& reporter,
        plasma::Transaction const& transaction,
        std::function<void()> notify):
      Operation{"download_files_for_" + transaction.id},
      _transaction_manager(transaction_manager),
      _network_manager(network_manager),
      _me(me),
      _reporter(reporter),
      _transaction(transaction),
      _notify{notify}
    {
      ELLE_TRACE_METHOD("");
    }

    void
    DownloadOperation::_run()
    {
      ELLE_DEBUG_METHOD("");

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

      ELLE_DEBUG("launch: %s %s",
                 transfer_binary,
                 boost::algorithm::join(arguments, " "));

      try
      {
        auto pc = binary_config("8transfer",
                                this->_me.id,
                                this->_transaction.network_id);
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
    DownloadOperation::_on_success()
    {
      ELLE_DEBUG_METHOD("");

      this->_transaction_manager.update(this->_transaction.id,
                                        plasma::TransactionStatus::finished);
      auto timestamp_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
      auto timestamp_tr = std::chrono::duration<double>(
        this->_transaction.timestamp);
      double duration = timestamp_now.count() - timestamp_tr.count();
      this->_reporter.store(
        "transaction_transferred",
        {{MKey::duration, std::to_string(duration)},
         {MKey::value, this->_transaction.id},
         {MKey::network, this->_transaction.network_id},
         {MKey::count, std::to_string(this->_transaction.files_count)},
         {MKey::size, std::to_string(this->_transaction.total_size)}});
    }

    void
    DownloadOperation::_on_error()
    {
      ELLE_DEBUG_METHOD("");

      this->_transaction_manager.update(this->_transaction.id,
                                        plasma::TransactionStatus::failed);
    }
  }
}
