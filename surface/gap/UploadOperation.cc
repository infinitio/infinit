#include "UploadOperation.hh"

#include <common/common.hh>

#include <elle/os/file.hh>
#include <elle/os/getenv.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("surface.gap.UploadOperation");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;

    static
    size_t
    total_size(std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(files);

      size_t size = 0;
      {
        for (auto const& file: files)
        {
          auto _size = elle::os::file::size(file);
          ELLE_DEBUG("%s: %i", file, _size);
          size += _size;
        }
      }
      return size;
    }

    UploadOperation::UploadOperation(
        TransactionManager& transaction_manager,
        NetworkManager& network_manager,
        plasma::meta::Client& meta,
        elle::metrics::Reporter& reporter,
        plasma::meta::SelfResponse& me,
        std::string const& device_id,
        std::string const& recipient_id_or_email,
        std::unordered_set<std::string> const& files):
      Operation{"upload_files_"},
      _transaction_manager(transaction_manager),
      _network_manager(network_manager),
      _reporter(reporter),
      _meta(meta),
      _me(me),
      _device_id(device_id),
      _recipient_id_or_email{recipient_id_or_email},
      _files{files}
    {}

    void
    UploadOperation::_run()
    {
      ELLE_TRACE_METHOD(this->_files);

      int size = total_size(this->_files);

      std::string first_file =
        fs::path(*(this->_files.cbegin())).filename().string();
      elle::utility::Time time; time.Current();
      std::string network_name = elle::sprintf("%s-%s",
                                               this->_recipient_id_or_email,
                                               time.nanoseconds);
      this->_network_id = this->_network_manager.create(network_name);
      plasma::meta::CreateTransactionResponse res;

      ELLE_DEBUG("(%s): (%s) %s [%s] -> %s throught %s (%s)",
                 this->_device_id,
                 this->_files.size(),
                 first_file,
                 size,
                 this->_recipient_id_or_email,
                 network_name,
                 this->_network_id);

      try
      {
        res = this->_meta.create_transaction(this->_recipient_id_or_email,
                                             first_file,
                                             this->_files.size(),
                                             size,
                                             fs::is_directory(first_file),
                                             this->_network_id,
                                             this->_device_id);
      }
      catch (...)
      {
        ELLE_DEBUG("transaction creation failed");
        // Something went wrong, we need to destroy the network.
        this->_network_manager.delete_(this->_network_id, false);
        throw;
      }
      this->_transaction_id = res.created_transaction_id;
      this->_name += this->_transaction_id;
      this->_me.remaining_invitations = res.remaining_invitations;

      this->_reporter.store(
        "transaction_create",
        {{MKey::status, "attempt"},
         {MKey::value, this->_transaction_id},
         {MKey::count, std::to_string(this->_files.size())},
         {MKey::size, std::to_string(size)}});

      ELLE_DEBUG("created network id is %s", this->_network_id);
      this->_network_manager.prepare(this->_network_id);
      this->_network_manager.to_directory(
        this->_network_id,
        common::infinit::network_shelter(
          this->_me.id,
          this->_network_id));

      this->_network_manager.wait_portal(this->_network_id);

      ELLE_DEBUG("Retrieving 8transfer binary path...");
      auto transfer_binary = common::infinit::binary_path("8transfer");
      ELLE_DEBUG("Using 8transfer binary '%s'", transfer_binary);

      try
      {
        try
        {
          for (auto& file: this->_files)
          {
            ELLE_DEBUG("uploading %s for operation %s", file, this->name());
            std::list<std::string> arguments{
              "-n", this->_network_id,
              "-u", this->_me.id,
              "--path", file,
              "--to",
            };

            ELLE_DEBUG("LAUNCH: %s %s",
                       transfer_binary,
                       boost::algorithm::join(arguments, " "));

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
                log_file += ".to.transfer.log";
                pc.setenv("ELLE_LOG_FILE", log_file);
              }
            }
            // set the environment and start the transfer
            elle::system::Process p{std::move(pc), transfer_binary, arguments};
            while (p.running())
            {
              if (p.wait_status(elle::system::Process::Milliseconds(100)) != 0)
                throw surface::gap::Exception(
                  gap_internal_error,
                  elle::sprintf("8transfer binary failed for network %s",
                                this->_network_id));

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
        }
        catch (...)
        {
          ELLE_DEBUG("detroying network");
          // Something went wrong, we need to destroy the network.
          this->_transaction_manager.update(
            this->_transaction_id,
            gap_TransactionStatus::gap_transaction_status_canceled);
          throw;
        }
      }
      CATCH_FAILURE_TO_METRICS("transaction_create");

      this->_reporter.store(
        "transaction_create",
        {{MKey::status, "succeed"},
         {MKey::value, this->_transaction_id},
         {MKey::count, std::to_string(this->_files.size())},
         {MKey::size, std::to_string(size)}});

      this->_transaction_manager.update(
        this->_transaction_id,
        gap_TransactionStatus::gap_transaction_status_created);
    }

    void
    UploadOperation::_cancel()
    {
      ELLE_DEBUG("cancelling %s name", this->name());
      this->_transaction_manager.update(
        this->_transaction_id,
        gap_TransactionStatus::gap_transaction_status_canceled);
    }
  }
}
