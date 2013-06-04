#include "PrepareTransactionOperation.hh"

#include <common/common.hh>

#include <elle/os/file.hh>
#include <elle/os/getenv.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("surface.gap.PrepareTransactionOperation");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;


    PrepareTransactionOperation::PrepareTransactionOperation(
        TransactionManager& transaction_manager,
        NetworkManager& network_manager,
        plasma::meta::Client& meta,
        elle::metrics::Reporter& reporter,
        plasma::meta::SelfResponse& self,
        Transaction const& transaction,
        std::unordered_set<std::string> const& files):
      Operation{"prepare_transaction_" + transaction.id},
      _transaction_manager(transaction_manager),
      _network_manager(network_manager),
      _meta(meta),
      _reporter(reporter),
      _self(self),
      _transaction(transaction),
      _files{files}
    {}

    void
    PrepareTransactionOperation::_run()
    {
      ELLE_TRACE_METHOD(this->_transaction);

      this->_network_manager.prepare(this->_transaction.network_id);
      this->_network_manager.to_directory(
        this->_transaction.network_id,
        common::infinit::network_shelter(this->_self.id,
                                         this->_transaction.network_id));

      this->_network_manager.wait_portal(this->_transaction.network_id);

      ELLE_DEBUG("giving '%s' access to the network '%s'",
                 this->_transaction.recipient_id,
                 this->_transaction.network_id);

      std::string recipient_k =
        this->_meta.user(this->_transaction.recipient_id).public_key;

      this->_network_manager.add_user(this->_transaction.network_id,
                                      this->_self.id,
                                      this->_transaction.recipient_id,
                                      recipient_k);

      ELLE_DEBUG("Giving '%s' permissions on the network to '%s'.",
                 this->_transaction.recipient_id,
                 this->_transaction.network_id);

      this->_network_manager.set_permissions(
        this->_transaction.network_id,
        this->_transaction.recipient_id,
        recipient_k,
        nucleus::neutron::permissions::write); // XXX write ?

      auto transfer_binary = common::infinit::binary_path("8transfer");

      try
      {
        for (auto& file: this->_files)
        {
          ELLE_DEBUG("uploading %s for operation %s", file, this->name());
          std::list<std::string> arguments{
            "-n", this->_transaction.network_id,
            "-u", this->_self.id,
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
      }
      CATCH_FAILURE_TO_METRICS("transaction_create");

      this->_reporter.store(
        "transaction_create",
        {{MKey::status, "succeed"},
         {MKey::value, this->_transaction.id},
         {MKey::count, std::to_string(this->_transaction.files_count)},
         {MKey::size, std::to_string(this->_transaction.total_size)}});

      this->_transaction_manager.update(this->_transaction.id,
                                        plasma::TransactionStatus::started);
    }

    void
    PrepareTransactionOperation::_cancel()
    {
      ELLE_DEBUG("cancelling %s name", this->name());
    }
  }
}

