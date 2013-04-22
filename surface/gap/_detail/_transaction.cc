#include "../TransactionManager.hh"
#include <surface/gap/metrics.hh>

#include <common/common.hh>

#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/system/Process.hh>
#include <elle/finally.hh>


#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {
    using MKey = elle::metrics::Key;
    namespace fs = boost::filesystem;

    size_t
    total_size(std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(files);

      size_t size = 0;
      ELLE_DEBUG("check files")
      {
        for (auto const& file: files)
        {
          auto _size = elle::os::path::size(file);
          ELLE_DEBUG("%s: %so", file, _size);
          size += _size;
        }
      }

      return size;
    }

    TransactionManager::TransactionManager(plasma::meta::Client& meta,
                                           Self& me):
      _meta(meta),
      _self(me)
    {}

    void
    TransactionManager::upload_files(std::string const& network_id,
                                     std::string const& owner,
                                     std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(network_id);

      ELLE_DEBUG("Retrieving 8transfert binary path...");
      auto transfer_binary = common::infinit::binary_path("8transfer");
      ELLE_DEBUG("Using 8transfert binary '%s'", transfer_binary);

      try
      {
        for (auto& file: files)
        {
          std::list<std::string> arguments{
            "-n", network_id,
            "-u", owner,
            "--path", file,
            "--to"
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
          if (p.wait_status() != 0)
            throw Exception(gap_internal_error, "8transfer binary failed");
        }
      }
      CATCH_FAILURE_TO_METRICS("transaction_create");
    }

    float
    TransactionManager::progress(std::string const& id,
                                 std::string const& user)
    {
      ELLE_TRACE("Retrieve progress of transaction %s", id);

      auto const& tr = this->one(id);

      if (tr.status == gap_transaction_status_finished)
        return 1.0f;
      else if (tr.status == gap_transaction_status_canceled)
        return 0.0f;
      else if (tr.status != gap_transaction_status_started)
        return 0.0f;
      // else if (!this->infinit_instance_manager().has_network(tr.network_id))
      // {
      //   throw Exception{
      //     gap_network_not_found,
      //       "Cannot launch 8progress without infinit instance"
      //       };
      // }

      std::string const& progress_binary = common::infinit::binary_path("8progress");
      std::list<std::string> arguments {"-n", tr.network_id,
                                        "-u", user
      };

      ELLE_DEBUG("LAUNCH: %s %s", progress_binary,
                 boost::algorithm::join(arguments, " "));

      auto pc = elle::system::process_config(elle::system::check_output_config);
      {
        std::string log_file = elle::os::getenv("INFINIT_LOG_FILE", "");

        if (!log_file.empty())
        {
          if (elle::os::in_env("INFINIT_LOG_FILE_PID"))
          {
            log_file += ".";
            log_file += std::to_string(::getpid());
          }
          log_file += ".progress.log";
          pc.setenv("ELLE_LOG_FILE", log_file);
        }
      }
      elle::system::Process p{std::move(pc), progress_binary, arguments};

      if (p.wait_status() != 0)
        throw Exception{gap_internal_error, "8progress binary failed"};

      int current_size = 0;
      int total_size = 0;
      std::stringstream ss;
      ss << p.read();
      ss >> current_size >> total_size;

      if (total_size == 0)
          return 0.f;
      float progress = float(current_size) / float(total_size);

      if (progress < 0)
      {
        ELLE_WARN("8progress returned a negative integer: %s", progress);
        progress = 0;
      }
      else if (progress > 1.0f)
      {
        ELLE_WARN("8progress returned an integer greater than 1: %s", progress);
        progress = 1.0f;
      }
      ELLE_DEBUG("transaction_progress(%s) -> %f", id, progress);
      return progress;
    }

    void
    TransactionManager::_download_files(std::string const& id,
                                        std::string const& output_dir)
    {
      auto pair = this->all().find(id);

      ELLE_ASSERT(pair != this->all().end());

      if (pair == this->all().end())
        return;

      Transaction const& trans = pair->second;

      std::string const& transfer_binary = common::infinit::binary_path("8transfer");

      std::list<std::string> arguments{
        "-n", trans.network_id,
        "-u", trans.recipient_id,
        "--path", output_dir,
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
          ELLE_DEBUG("Waiting transfert process to finish");
          p.wait();
        }

        if (trans.files_count == 1)
        {
          ELLE_LOG("Download complete. Your file is at '%s'.",
                   elle::os::path::join(this->_output_dir.c_str(),
                                        trans.first_filename));
        }
        else
        {
          ELLE_LOG("Download complete. Your %d files are in '%s'.",
              trans.files_count, this->_output_dir.c_str());
        }

        this->update_transaction(
            id,
            gap_TransactionStatus::gap_transaction_status_finished
        );
      }
      catch (std::exception const& err)
      {
        ELLE_ERR("couldn't receive file %s: %s", trans.first_filename,
                                                 err.what());
        this->update_transaction(
            id,
            gap_TransactionStatus::gap_transaction_status_canceled
        );
        this->_cancel_operation("download_files_for_" + id);
      }
    }

    static
    bool
    _check_action_is_available(std::string const& user_id,
                               Transaction const& transaction,
                               gap_TransactionStatus status)
    {
      typedef
        std::map<gap_TransactionStatus, std::set<gap_TransactionStatus>>
        StatusMap;

      static const StatusMap _sender_status_update{
        {gap_transaction_status_pending,
          {gap_transaction_status_canceled, gap_transaction_status_created}},
        {gap_transaction_status_created,
          {gap_transaction_status_canceled, gap_transaction_status_prepared}},
        {gap_transaction_status_accepted,
          {gap_transaction_status_prepared, gap_transaction_status_created, gap_transaction_status_canceled}},
        {gap_transaction_status_prepared,
          {gap_transaction_status_canceled}},
        {gap_transaction_status_started,
          {gap_transaction_status_canceled}},
        // {gap_transaction_status_canceled,
        //   {}},
        // {gap_transaction_status_finished,
        //   {}}
      };

      static StatusMap _recipient_status_update{
        {gap_transaction_status_pending,
          {gap_transaction_status_accepted, gap_transaction_status_canceled}},
        {gap_transaction_status_created,
          {gap_transaction_status_accepted, gap_transaction_status_canceled}},
        {gap_transaction_status_accepted,
          {gap_transaction_status_canceled}},
        {gap_transaction_status_prepared,
          {gap_transaction_status_started, gap_transaction_status_canceled}},
        {gap_transaction_status_started,
          {gap_transaction_status_canceled, gap_transaction_status_finished}},
        // {gap_transaction_status_canceled,
        //   {}},
        // {gap_transaction_status_finished,
        //   {}}
      };

      if (user_id != transaction.recipient_id &&
          user_id != transaction.sender_id)
      {
        throw Exception{gap_error, "You are neither recipient nor the sender."};
      }

      auto list = (user_id == transaction.recipient_id) ? _recipient_status_update
                                                        : _sender_status_update;

      auto const& status_list = list.find(
        (gap_TransactionStatus) transaction.status);

      if (status_list == list.end() ||
          status_list->second.find((gap_TransactionStatus) status) == status_list->second.end())
      {
        ELLE_WARN("You are not allowed to change status from %s to %s",
                  transaction.status, status);
          return false;
      }

      return true;
    }

    void
    TransactionManager::update_transaction(std::string const& id,
                                           gap_TransactionStatus status)
    {
      ELLE_DEBUG("Update transaction '%s': '%s'",
                 id, status);

      auto pair = this->all().find(id);

      ELLE_ASSERT(pair != this->all().end());

      if (pair == this->all().end())
        return;

      Transaction const& transaction = pair->second;

      if (!_check_action_is_available(this->_me._id, transaction, status))
        throw Exception(gap_api_error,
                        elle::sprintf("you are allowed to change transaction " \
                                      " status from %s to %s",
                                      transaction.status, status));

      switch (status)
      {
        case gap_transaction_status_created:
          this->_create_transaction(transaction);
          break;
        case gap_transaction_status_accepted:
          this->_accept_transaction(transaction);
          break;
        case gap_transaction_status_prepared:
          this->_prepare_transaction(transaction);
          break;
        case gap_transaction_status_started:
          this->_start_transaction(transaction);
          break;
        case gap_transaction_status_canceled:
          this->_cancel_transaction(transaction);
          break;
        case gap_transaction_status_finished:
          this->_close_transaction(transaction);
          break;
        default:
          ELLE_WARN("Status %s doesn't exist", status);
          throw Exception(gap_api_error,
                          elle::sprintf("unknow status %s", status));

          return;
      }

      // Send file request successful.
    } // !update_transaction()

    void
    TransactionManager::_create_transaction(Transaction const& transaction)
    {
      if (transaction.sender_device_id != this->device_id())
        throw Exception{gap_error, "Only sender can lekf his network."};

    }

    void
    TransactionManager::_on_transaction_created(Transaction const& transaction)
    {
      if (transaction.sender_device_id != this->device_id())
        return;

      if (transaction.already_accepted)
        this->update_transaction(transaction.id,
                                 gap_TransactionStatus::gap_transaction_status_prepared);
    }

    void
    TransactionManager::_accept_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Accept transaction '%s'", transaction.id);

      if (transaction.recipient_id != this->_me._id)
      {
        throw Exception{gap_error, "Only recipient can accept transaction."};
      }

      reporter().store("transaction_accept",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.id}});

      try
      {
        ELLE_ERR("%s %s", this->device_id(), this->device_name());
        this->_meta.update_transaction(transaction.id,
                                        plasma::TransactionStatus::accepted,
                                        this->device_id(),
                                        this->device_name());
      }
      CATCH_FAILURE_TO_METRICS("transaction_accept");

      reporter().store("transaction_accept",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});

      // Could be improve.
      _swaggers_dirty = true;
    }

    void
    TransactionManager::_on_transaction_accepted(Transaction const& transaction)
    {
      ELLE_TRACE("On transaction accepted '%s'", transaction.id);

      if (transaction.sender_device_id != this->device_id())
        return;

      if (!transaction.already_accepted)
      {
        // When recipient has rights, allow him to start download.
        this->update_transaction(transaction.id,
                                 gap_transaction_status_prepared);

        // XXX Could be improved.
        _swaggers_dirty = true;
      }
    }

    void
    TransactionManager::_prepare_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_FUNCTION(transaction.id);

      if (transaction.sender_device_id != this->device_id())
      {
        throw Exception{gap_error, "Only sender can prepare his network."};
      }

      reporter().store("transaction_ready",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.id}});

      try
      {
        if (this->_wait_portal(transaction.network_id) == false)
          throw Exception{gap_network_error, "Cannot wait portal"};


        ELLE_ERR("============================================================");
        ELLE_ERR("Giving '%s' access to the network '%s'",
                   transaction.recipient_id,
                   transaction.network_id);

        this->network_manager().networks_dirty = true;
        this->network_manager().add_user(transaction.network_id,
                                         this->me()._id,
                                         transaction.recipient_id);

        ELLE_ERR("Giving '%s' permissions on the network to '%s'.",
                 transaction.recipient_id,
                 transaction.network_id);

        this->set_permissions(transaction.recipient_id,
                              transaction.network_id,
                              nucleus::neutron::permissions::write);
        ELLE_ERR("============================================================");

        this->_meta.update_transaction(transaction.id,
                                        plasma::TransactionStatus::prepared);
      }
      CATCH_FAILURE_TO_METRICS("transaction_ready");

      reporter().store("transaction_ready",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_prepared(Transaction const& transaction)
    {
      ELLE_TRACE("prepared trans '%s'", transaction.id);

      if (transaction.recipient_device_id != this->device_id())
      {
        ELLE_DEBUG("transaction doesn't concern your device.");
        return;
      }

      this->network_manager().networks_dirty = true;
      this->_meta.network_add_device(transaction.network_id, this->device_id());

      this->network_manager().prepare(transaction.network_id);

      this->update_transaction(transaction.id,
                               gap_transaction_status_started);
    }

    void
    TransactionManager::_start_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Start transaction '%s'", transaction.id);

      if (transaction.recipient_device_id != this->device_id())
      {
        throw Exception{gap_error, "Only recipient can start transaction."};
      }

      reporter().store("transaction_start",
                            {{MKey::status, "attempt"},
                              {MKey::value, transaction.id}});

      try
      {
        this->_meta.update_transaction(transaction.id,
                                        plasma::TransactionStatus::started);
      }
      CATCH_FAILURE_TO_METRICS("transaction_start");

      reporter().store("transaction_start",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});

    }

    void
    TransactionManager::_on_transaction_started(Transaction const& transaction)
    {
      ELLE_TRACE("Started trans '%s'", transaction.id);

      if (transaction.recipient_device_id != this->device_id() &&
          transaction.sender_device_id != this->device_id())
      {
        ELLE_DEBUG("transaction doesn't concern your device.");
        return;
      }

      if (this->_wait_portal(transaction.network_id) == false)
        throw Exception{gap_error, "Couldn't find portal to infinit instance"};

      std::exception_ptr exception;
      {
        reactor::Scheduler sched;
        reactor::Thread sync{
          sched,
          "notify_8infinit",
          [&]
          {
            try
            {
              this->_notify_8infinit(transaction, sched);
            }
            catch (elle::Exception const& e)
            {
              exception = std::make_exception_ptr(e);
            }
          }
        };

        sched.run();
        ELLE_DEBUG("notify finished");
      }

      if (exception != std::exception_ptr{})
      {
        std::string msg;
        try { std::rethrow_exception(exception); }
        catch (std::exception const& err) { msg = err.what(); }
        catch (...) { msg = "Unknown exception type"; }
        ELLE_ERR("cannot connect infinit instances: %s", msg);
        this->update_transaction(transaction.id,
                                 gap_transaction_status_canceled);
        std::rethrow_exception(exception);
      }
      if (transaction.recipient_device_id == this->device_id())
      {
        this->_add_operation("download_files_for_" + transaction.id,
                             std::bind(&TransactionManager::_download_files,
                                       this,
                                       transaction.id),
                             true);
      }
    }

    void
    TransactionManager::_close_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Close transaction '%s'", transaction.id);

      if(transaction.recipient_device_id != this->device_id())
      {
        throw Exception{gap_error,
            "Only recipient can close transaction."};
      }

      reporter().store("transaction_finish",
                            {{MKey::status, "attempt"},
                              {MKey::value, transaction.id}});

      try
      {
        this->_meta.update_transaction(transaction.id,
                                        plasma::TransactionStatus::finished);
      }
      CATCH_FAILURE_TO_METRICS("transaction_finish");

      reporter().store("transaction_finish",
                            {{MKey::status, "succeed"},
                              {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_closed(Transaction const& transaction)
    {
      ELLE_DEBUG("Closed transaction '%s'", transaction.id);

      // Delete networks.
      this->network_manager().delete_(transaction.network_id, true /* force */);
    }

    void
    TransactionManager::_cancel_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Cancel transaction '%s'", transaction.id);

      //XXX: If download has started, cancel it, delete files, ...

      std::string author{
        transaction.sender_id == this->_me._id ? "sender" : "recipient",};

      reporter().store("transaction_cancel",
                            {{MKey::status, "attempt"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.id}});

      ELLE_SCOPE_EXIT(
        [&] { this->_cancel_all_operations(transaction.id);
              this->network_manager().delete_(transaction.network_id, true); } );

      try
      {
        this->_meta.update_transaction(transaction.id,
                                        plasma::TransactionStatus::canceled);
      }
      CATCH_FAILURE_TO_METRICS("transaction_cancel");

      reporter().store("transaction_cancel",
                            {{MKey::status, "succeed"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_canceled(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD("Canceled transaction '%s'", transaction.id);

      // XXX: If some operation are launched, such as 8transfer, 8progess for the
      // current transaction, cancel them.
      this->_cancel_all_operations(transaction.id);
      this->infinit_instance_manager().stop_network(transaction.network_id);

      // Delete networks.
      this->network_manager().delete_(transaction.network_id, true);
      ELLE_DEBUG("Network %s successfully deleted", transaction.network_id);
    }

    TransactionManager::TransactionsMap const&
    TransactionManager::all()
    {
      if (_transactions != nullptr)
        return *_transactions;

      _transactions.reset(new TransactionsMap{});

      auto response = this->_meta.transactions();
      for (auto const& id: response.transactions)
        {
          auto transaction = this->_meta.transaction(id);
          (*this->_transactions)[id] = transaction;
        }

      return *(this->_transactions);
    }

    Transaction const&
    TransactionManager::one(std::string const& id)
    {
      auto it = this->all().find(id);
      if (it != this->all().end())
        return it->second;
      return this->transaction_sync(id);
    }

    Transaction const&
    TransactionManager::sync(std::string const& id)
    {
      ELLE_TRACE("Synching transaction %s from _meta", id);
      this->all(); // ensure _transactions is not null;
      try
      {
        auto transaction = this->_meta.transaction(id);
        ELLE_DEBUG("Synched transaction %s has status %d",
                   id, transaction.status);
        return ((*this->_transactions)[id] = transaction);
      }
      catch (std::runtime_error const& e)
      {
        throw Exception{gap_transaction_doesnt_exist, e.what()};
      }
    }
  }
}
