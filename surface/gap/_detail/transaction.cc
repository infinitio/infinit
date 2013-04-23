#include "../State.hh"
#include <metrics/Reporter.hh>

#include <common/common.hh>

#include <elle/utility/Time.hh>
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

    State::OperationId
    State::send_files(std::string const& recipient_id_or_email,
                      std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(recipient_id_or_email, files);

      if (files.empty())
        throw Exception(gap_no_file, "no files to send");

      int size = 0;
      for (auto const& file: files)
        size += this->file_size(file);

      std::string first_file = fs::path(*(files.cbegin())).filename().string();
      elle::utility::Time time; time.Current();
      std::string network_name = elle::sprintf("%s-%s",
                                               recipient_id_or_email,
                                               time.nanoseconds);
      std::string network_id = this->create_network(network_name);
      plasma::meta::CreateTransactionResponse res;

      ELLE_DEBUG("(%s): (%s) %s [%s] -> %s throught %s (%s)",
                 this->device_id(),
                 files.size(),
                 first_file,
                 size,
                 recipient_id_or_email,
                 network_name,
                 network_id);

      try
      {
        res = this->_meta->create_transaction(recipient_id_or_email,
                                              first_file,
                                              files.size(),
                                              size,
                                              fs::is_directory(first_file),
                                              network_id,
                                              this->_device_id);
      }
      catch (...)
      {
        ELLE_DEBUG("transaction creation failed");
        // Something went wrong, we need to destroy the network.
        this->delete_network(network_id, false);
        throw;
      }

      this->_me.remaining_invitations = res.remaining_invitations;

      return this->_add_operation(
        "send_files_for_" + res.created_transaction_id,
        [&,network_id,res,files]
        {
          this->_reporter.store("transaction_create",
                                {{MKey::status, "attempt"},
                                  {MKey::value, res.created_transaction_id},
                                  {MKey::count, std::to_string(files.size())},
                                  {MKey::size, std::to_string(size)}});

          try
          {
            this->_upload_files(network_id, files);
          }
          catch (...)
          {
            ELLE_DEBUG("detroying network");
            // Something went wrong, we need to destroy the network.
            this->delete_network(network_id, false);
            throw;
          }

          this->_reporter.store("transaction_create",
                                {{MKey::status, "succeed"},
                                  {MKey::value, res.created_transaction_id},
                                  {MKey::count, std::to_string(files.size())},
                                  {MKey::size, std::to_string(size)}});

          this->update_transaction(res.created_transaction_id,
                                   gap_TransactionStatus::gap_transaction_status_created);
        });
    }

    void
    State::_upload_files(std::string const& network_id,
                       std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(network_id, files);

      ELLE_DEBUG("created network id is %s", network_id);
      if (this->_wait_portal(network_id) == false)
          throw Exception{gap_error, "Couldn't find portal to infinit instance"};

      ELLE_DEBUG("Retrieving 8transfert binary path...");
      auto transfer_binary = common::infinit::binary_path("8transfer");
      ELLE_DEBUG("Using 8transfert binary '%s'", transfer_binary);

      try
      {
        for (auto& file: files)
        {
          std::list<std::string> arguments{
            "-n", network_id,
            "-u", this->_me._id,
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
    State::transaction_progress(std::string const& transaction_id)
    {
      ELLE_TRACE("Retrieve progress of transaction %s", transaction_id);

      auto const& tr = this->transaction(transaction_id);

      if (tr.status == gap_transaction_status_finished)
        return 1.0f;
      else if (tr.status == gap_transaction_status_canceled)
        return 0.0f;
      else if (tr.status != gap_transaction_status_started)
        return 0.0f;
      else if (!this->infinit_instance_manager().has_network(tr.network_id))
        {
          throw Exception{
              gap_network_not_found,
              "Cannot launch 8progress without infinit instance"
          };
        }

      std::string const& progress_binary = common::infinit::binary_path("8progress");
      std::list<std::string> arguments{"-n", tr.network_id, "-u", this->_me._id};

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

      if (p.wait_status(elle::system::Process::Milliseconds(10)) != 0)
        throw Exception{
            gap_internal_error, "8progress binary failed"
        };
      if (p.running())
      {
        p.kill();
        p.wait();
      }

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
      ELLE_DEBUG("transaction_progress(%s) -> %f", transaction_id, progress);
      return progress;
    }

    void
    State::_download_files(std::string const& transaction_id)
    {
      auto pair = State::transactions().find(transaction_id);

      ELLE_ASSERT(pair != State::transactions().end());

      if (pair == State::transactions().end())
        return;

      Transaction const& trans = pair->second;

      std::string const& transfer_binary = common::infinit::binary_path("8transfer");

      std::list<std::string> arguments{
        "-n", trans.network_id,
        "-u", this->_me._id,
        "--path", this->_output_dir,
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
              elle::os::path::join(
                this->_output_dir.c_str(), trans.first_filename
              )
          );
        }
        else
        {
          ELLE_LOG("Download complete. Your %d files are in '%s'.",
              trans.files_count, this->_output_dir.c_str());
        }

        this->update_transaction(
            transaction_id,
            gap_TransactionStatus::gap_transaction_status_finished
        );
      }
      catch (std::exception const& err)
      {
        ELLE_ERR("couldn't receive file %s: %s", trans.first_filename,
                                                 err.what());
        this->update_transaction(
            transaction_id,
            gap_TransactionStatus::gap_transaction_status_canceled
        );
        this->_cancel_operation("download_files_for_" + transaction_id);
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
    State::update_transaction(std::string const& transaction_id,
                              gap_TransactionStatus status)
    {
      ELLE_DEBUG("Update transaction '%s': '%s'",
                 transaction_id, status);

      auto pair = State::transactions().find(transaction_id);

      ELLE_ASSERT(pair != State::transactions().end());

      if (pair == State::transactions().end())
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
    State::_create_transaction(Transaction const& transaction)
    {
      if (transaction.sender_device_id != this->device_id())
        throw Exception{gap_error, "Only sender can lekf his network."};

      this->_meta->update_transaction(transaction.transaction_id,
                                      plasma::TransactionStatus::created);
    }

    void
    State::_on_transaction_created(Transaction const& transaction)
    {
      if (transaction.sender_device_id != this->_device_id)
        return;

      if (transaction.already_accepted)
      {
        ELLE_DEBUG("the transaction %s has already been accepted",
                   transaction.transaction_id);

        this->update_transaction(transaction.transaction_id,
                                 gap_TransactionStatus::gap_transaction_status_prepared);
      }
    }

    void
    State::_accept_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Accept transaction '%s'", transaction.transaction_id);

      if (transaction.recipient_id != this->_me._id)
      {
        throw Exception{gap_error, "Only recipient can accept transaction."};
      }

      this->_reporter.store("transaction_accept",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.transaction_id}});

      try
      {
        this->_meta->update_transaction(transaction.transaction_id,
                                        plasma::TransactionStatus::accepted,
                                        this->device_id(),
                                        this->device_name());
      }
      CATCH_FAILURE_TO_METRICS("transaction_accept");

      this->_reporter.store("transaction_accept",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.transaction_id}});

      // Could be improve.
      _swaggers_dirty = true;
    }

    void
    State::_on_transaction_accepted(Transaction const& transaction)
    {
      ELLE_TRACE("On transaction accepted '%s'", transaction.transaction_id);

      if (transaction.sender_device_id != this->device_id())
        return;

      if (!transaction.already_accepted)
      {
        // When recipient has rights, allow him to start download.
        this->update_transaction(transaction.transaction_id,
                                 gap_transaction_status_prepared);

        // XXX Could be improved.
        _swaggers_dirty = true;
      }
    }

    void
    State::_prepare_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_FUNCTION(transaction.transaction_id);

      if (transaction.sender_device_id != this->device_id())
      {
        throw Exception{gap_error, "Only sender can prepare his network."};
      }

      this->_reporter.store("transaction_ready",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.transaction_id}});

      try
      {
        if (this->_wait_portal(transaction.network_id) == false)
          throw Exception{gap_network_error, "Cannot wait portal"};

        ELLE_DEBUG("giving '%s' access to the network '%s'",
                   transaction.recipient_id,
                   transaction.network_id);

        this->_networks_dirty = true;
        this->network_add_user(transaction.network_id,
                               transaction.recipient_id);

        ELLE_DEBUG("Giving '%s' permissions on the network to '%s'.",
                 transaction.recipient_id,
                   transaction.network_id);

        this->set_permissions(transaction.recipient_id,
                              transaction.network_id,
                              nucleus::neutron::permissions::write);


        this->_meta->update_transaction(transaction.transaction_id,
                                        plasma::TransactionStatus::prepared);
      }
      CATCH_FAILURE_TO_METRICS("transaction_ready");

      this->_reporter.store("transaction_ready",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.transaction_id}});
    }

    void
    State::_on_transaction_prepared(Transaction const& transaction)
    {
      ELLE_TRACE("prepared trans '%s'", transaction.transaction_id);

      if (transaction.recipient_device_id != this->device_id())
      {
        ELLE_DEBUG("transaction doesn't concern your device.");
        return;
      }

      this->_networks_dirty = true;
      this->prepare_network(transaction.network_id);

      this->_meta->network_add_device(
        transaction.network_id, this->device_id());

      this->infinit_instance_manager().launch_network(transaction.network_id);

      if (this->_wait_portal(transaction.network_id) == false)
        throw Exception{gap_network_error, "Cannot wait portal"};

      this->update_transaction(transaction.transaction_id,
                               gap_transaction_status_started);
    }

    void
    State::_start_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Start transaction '%s'", transaction.transaction_id);

      if (transaction.recipient_device_id != this->device_id())
      {
        throw Exception{gap_error, "Only recipient can start transaction."};
      }

      this->_reporter.store("transaction_start",
                            {{MKey::status, "attempt"},
                              {MKey::value, transaction.transaction_id}});

      try
      {
        this->_meta->update_transaction(transaction.transaction_id,
                                        plasma::TransactionStatus::started);
      }
      CATCH_FAILURE_TO_METRICS("transaction_start");

      this->_reporter.store("transaction_start",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.transaction_id}});

    }

    void
    State::_on_transaction_started(Transaction const& transaction)
    {
      ELLE_TRACE("Started trans '%s'", transaction.transaction_id);

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
        this->update_transaction(transaction.transaction_id,
                                 gap_transaction_status_canceled);
        std::rethrow_exception(exception);
      }
      if (transaction.recipient_device_id == this->device_id())
      {
        this->_add_operation("download_files_for_" + transaction.transaction_id,
                             std::bind(&State::_download_files,
                                       this,
                                       transaction.transaction_id),
                             true);
      }
    }

    void
    State::_close_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Close transaction '%s'", transaction.transaction_id);

      if(transaction.recipient_device_id != this->device_id())
      {
        throw Exception{gap_error,
            "Only recipient can close transaction."};
      }

      this->_reporter.store("transaction_finish",
                            {{MKey::status, "attempt"},
                              {MKey::value, transaction.transaction_id}});

      try
      {
        this->_meta->update_transaction(transaction.transaction_id,
                                        plasma::TransactionStatus::finished);
      }
      CATCH_FAILURE_TO_METRICS("transaction_finish");

      this->_reporter.store("transaction_finish",
                            {{MKey::status, "succeed"},
                              {MKey::value, transaction.transaction_id}});
    }

    void
    State::_on_transaction_closed(Transaction const& transaction)
    {
      ELLE_DEBUG("Closed transaction '%s'", transaction.transaction_id);

      // Delete networks.
      this->delete_network(transaction.network_id, true /* force */);
    }

    void
    State::_cancel_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG("Cancel transaction '%s'", transaction.transaction_id);

      //XXX: If download has started, cancel it, delete files, ...

      std::string author{
        transaction.sender_id == this->_me._id ? "sender" : "recipient",};

      this->_reporter.store("transaction_cancel",
                            {{MKey::status, "attempt"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.transaction_id}});

      ELLE_SCOPE_EXIT(
        [&] { this->delete_network(transaction.network_id, true); }
      );

      try
      {
        this->_meta->update_transaction(transaction.transaction_id,
                                        plasma::TransactionStatus::canceled);
      }
      CATCH_FAILURE_TO_METRICS("transaction_cancel");

      this->_reporter.store("transaction_cancel",
                            {{MKey::status, "succeed"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.transaction_id}});
    }

    void
    State::_on_transaction_canceled(Transaction const& transaction)
    {
      ELLE_DEBUG("Canceled transaction '%s'", transaction.transaction_id);

      // XXX: If some operation are launched, such as 8transfer, 8progess for the
      // current transaction, cancel them.
      this->infinit_instance_manager().stop_network(transaction.network_id);

      // Delete networks.
      this->delete_network(transaction.network_id, true);
      ELLE_DEBUG("Network %s successfully deleted", transaction.network_id);
    }

    State::TransactionsMap const&
    State::transactions()
    {
      if (_transactions != nullptr)
        return *_transactions;

      _transactions.reset(new TransactionsMap{});

      auto response = this->_meta->transactions();
      for (auto const& transaction_id: response.transactions)
        {
          auto transaction = this->_meta->transaction(transaction_id);
          (*this->_transactions)[transaction_id] = transaction;
        }

      return *(this->_transactions);
    }

    Transaction const&
    State::transaction(std::string const& id)
    {
      auto it = this->transactions().find(id);
      if (it != this->transactions().end())
        return it->second;
      return this->transaction_sync(id);
    }

    Transaction const&
    State::transaction_sync(std::string const& id)
    {
      ELLE_TRACE("Synching transaction %s from meta", id);
      this->transactions(); // ensure _transactions is not null;
      try
        {
          auto transaction = this->_meta->transaction(id);
          ELLE_DEBUG("Synched transaction %s has status %d",
                     id, transaction.status);
          return ((*this->_transactions)[id] = transaction);
        }
      catch (std::runtime_error const& e)
        {
          throw Exception{gap_transaction_doesnt_exist, e.what()};
        }
    }

    void
    State::transaction_callback(TransactionNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool is_new) -> void {
        return cb(static_cast<TransactionNotification const&>(notif), is_new);
      };

      this->_notification_handlers[NotificationType::transaction].push_back(fn);
    }

    void
    State::transaction_status_callback(TransactionStatusNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool is_new) -> void {
        return cb(static_cast<TransactionStatusNotification const&>(notif), is_new);
      };

      _notification_handlers[NotificationType::transaction_status].push_back(fn);
    }

    void
    State::message_callback(MessageNotificationCallback const& cb)
    {
      auto fn = [cb] (Notification const& notif, bool) -> void {
        return cb(static_cast<MessageNotification const&>(notif));
      };

      this->_notification_handlers[NotificationType::message].push_back(fn);
    }

    void
    State::_on_transaction(TransactionNotification const& notif,
                           bool is_new)
    {
      ELLE_TRACE_FUNCTION(notif.transaction.transaction_id, is_new);

      // If it's not new, we already has it on our transactions.
      if (!is_new) return;

      auto it = this->transactions().find(notif.transaction.transaction_id);

      if (it != this->transactions().end())
      {
        // The evaluation of transaction is lazy, which means that if your first
        // operation about transactions is create one, at the first evaluation,
        // the new transaction will already be in the transactions map, causing
        // the following warning to appear. Don't care.
        ELLE_WARN("you already have this transaction");
        return;
      }

      // Normal case, this is a new transaction, store it to match server.
      (*this->_transactions)[notif.transaction.transaction_id] = notif.transaction;
    }

    void
    State::_on_transaction_status(TransactionStatusNotification const& notif)
    {
      ELLE_TRACE_FUNCTION(notif.status);

      auto const pair = State::transactions().find(notif.transaction_id);

      if (pair == State::transactions().end())
      {
        // Something went wrong.
        auto transaction = this->_meta->transaction(notif.transaction_id);

        if (transaction.status == gap_transaction_status_canceled)
        {
          ELLE_WARN("we merged a canceled transaction, nothing to do with that.");
          return;
        }

        (*this->_transactions)[notif.transaction_id] = transaction;
      }

      this->_transactions->at(notif.transaction_id).status = notif.status;

      auto const& transaction = this->transaction(notif.transaction_id);

      switch((plasma::TransactionStatus) notif.status)
      {
        case plasma::TransactionStatus::created:
          // We update the transaction from meta.
          // XXX: we should have it from transaction_notification.
          (*_transactions)[notif.transaction_id] = this->_meta->transaction(
              notif.transaction_id
          );
          this->_on_transaction_created(transaction);
          break;
        case plasma::TransactionStatus::accepted:
          // We update the transaction from meta.
          // XXX: we should have it from transaction_notification.
          (*_transactions)[notif.transaction_id] = this->_meta->transaction(
              notif.transaction_id
          );
          this->_on_transaction_accepted(transaction);
          break;
        case plasma::TransactionStatus::prepared:
          this->_on_transaction_prepared(transaction);
          break;
        case plasma::TransactionStatus::started:
          this->_on_transaction_started(transaction);
          break;
        case plasma::TransactionStatus::canceled:
          this->_on_transaction_canceled(transaction);
          break;
        case plasma::TransactionStatus::finished:
          this->_on_transaction_closed(transaction);
          break;
        default:
          ELLE_WARN("The status '%s' is unknown.", notif.status);
          return;
      }
    }

  }
}
