#include "PrepareTransactionOperation.hh"
#include "binary_config.hh"

#include <common/common.hh>

#include <metrics/Reporter.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.PrepareTransactionOperation");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;


    PrepareTransactionOperation::PrepareTransactionOperation(
        TransactionManager& transaction_manager,
        NetworkManager& network_manager,
        plasma::meta::Client& meta,
        metrics::Reporter& reporter,
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
    {
      ELLE_TRACE_METHOD("");
    }

    void
    PrepareTransactionOperation::_run()
    {
      ELLE_DEBUG_METHOD("");

      ELLE_DEBUG("running transaction '%s'", this->_transaction);

      this->_reporter.store(
        "preparing",
        {
          {MKey::value, this->_transaction.id},
          {MKey::count, std::to_string(this->_transaction.files_count)},
          {MKey::size, std::to_string(this->_transaction.total_size)},
          {MKey::timestamp, std::to_string(this->_transaction.files_count)},
        });

      ELLE_DEBUG("prepare network and directories for %s",
                 this->_transaction.network_id)
      {
        this->_network_manager.prepare(this->_transaction.network_id);
        this->_network_manager.to_directory(
          this->_transaction.network_id,
          common::infinit::network_shelter(this->_self.id,
                                           this->_transaction.network_id));
      }

      this->_network_manager.launch(this->_transaction.network_id);

      ELLE_DEBUG("giving '%s' access to the network '%s'",
                 this->_transaction.recipient_id,
                 this->_transaction.network_id);

      std::string recipient_K =
        this->_meta.user(this->_transaction.recipient_id).public_key;
      ELLE_ASSERT_NEQ(recipient_K.size(), 0u);

      this->_network_manager.add_user(this->_transaction.network_id,
                                      recipient_K);

      ELLE_DEBUG("Giving '%s' permissions on the network to '%s'.",
                 this->_transaction.recipient_id,
                 this->_transaction.network_id);

      this->_network_manager.set_permissions(this->_transaction.network_id,
                                             recipient_K);

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

          ELLE_DEBUG("launch: %s %s",
                     transfer_binary,
                     boost::algorithm::join(arguments, " "));

          auto pc = binary_config("8transfer",
                                  this->_self.id,
                                  this->_transaction.network_id);
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
      CATCH_FAILURE_TO_METRICS("transaction_preparing");

      this->_reporter.store(
        "transaction_prepared",
        {{MKey::value, this->_transaction.id},
         {MKey::network, this->_transaction.network_id},
         {MKey::count, std::to_string(this->_transaction.files_count)},
         {MKey::size, std::to_string(this->_transaction.total_size)}});

      this->_transaction_manager.update(this->_transaction.id,
                                        plasma::TransactionStatus::started);
    }

    void
    PrepareTransactionOperation::_cancel()
    {
      ELLE_DEBUG_METHOD("");

      ELLE_DEBUG("cancelling transaction '%s'", this->name());
    }

    void
    PrepareTransactionOperation::_on_error()
    {
      this->_transaction_manager.update(this->_transaction.id,
                                        plasma::TransactionStatus::failed);
    }
  }
}
