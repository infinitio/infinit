#include "TransactionManager.hh"

#include "binary_config.hh"
#include "CreateTransactionOperation.hh"
#include "DownloadOperation.hh"
#include "PrepareTransactionOperation.hh"
#include "UploadOperation.hh"
#include "metrics.hh"

#include <plasma/meta/Client.hh>

#include <common/common.hh>

#include <elle/os/path.hh>
#include <elle/os/file.hh>
#include <elle/os/getenv.hh>
#include <elle/system/Process.hh>
#include <elle/memory.hh>
#include <elle/finally.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

#include <chrono>

ELLE_LOG_COMPONENT("infinit.surface.gap.Transaction");

namespace surface
{
  namespace gap
  {
    using MKey = elle::metrics::Key;
    namespace fs = boost::filesystem;

    struct TransactionManager::TransactionProgress
    {
    public:
      float last_value;
      std::unique_ptr<elle::system::Process> process;

    public:
      TransactionProgress():
        last_value{0.0f}
      {}
    };

    TransactionManager::TransactionManager(NotificationManager&
                                           notification_manager,
                                           NetworkManager& network_manager,
                                           UserManager& user_manager,
                                           plasma::meta::Client& meta,
                                           elle::metrics::Reporter& reporter,
                                           Self& self,
                                           Device const& device):
      Notifiable(notification_manager),
      _network_manager(network_manager),
      _user_manager(user_manager),
      _meta(meta),
      _reporter(reporter),
      _self(self),
      _device(device),
      _output_dir{common::system::download_directory()}
    {
      ELLE_TRACE_METHOD("");

      this->_notification_manager.transaction_callback(
        [&] (TransactionNotification const &n, bool) -> void
        {
          this->_on_transaction(n);
        });
      this->_notification_manager.user_status_callback(
        [&] (UserStatusNotification const &n) -> void
        {
          this->_on_user_status(n);
        });
    }

    TransactionManager::~TransactionManager()
    {
      ELLE_TRACE_METHOD("");

      try
      {
        ELLE_TRACE("destroying the transaction manager");
        this->clear();
      }
      catch (...)
      {
        ELLE_WARN("couldn't clear the transaction manager: %s",
                  elle::exception_string());
      }
    }

    void
    TransactionManager::clear()
    {
      ELLE_TRACE_METHOD("");

      this->_network_manager.clear();
    }

    void
    TransactionManager::output_dir(std::string const& dir)
    {
      ELLE_TRACE_METHOD(dir);

      if (!fs::exists(dir))
        throw Exception{gap_error,
                        "directory doesn't exist."};

      if (!fs::is_directory(dir))
        throw Exception{gap_error,
                        "not a directroy."};

      this->_output_dir = dir;
    }

    OperationManager::OperationId
    TransactionManager::send_files(std::string const& recipient_id_or_email,
                                   std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_METHOD(recipient_id_or_email, files);

      if (files.empty())
        throw Exception(gap_no_file, "no files to send");

      return this->_add<CreateTransactionOperation>(
          *this,
          this->_network_manager,
          this->_user_manager,
          this->_meta,
          this->_reporter,
          this->_self,
          this->_device.id,
          recipient_id_or_email,
          files,
          [this, files] (std::string const& tr_id) {
            this->_states([&tr_id, &files] (StateMap& map) {
              map[tr_id].files = files;
            });
          });
    }

    float
    TransactionManager::progress(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

      auto const& tr = this->one(id);
      auto const& instance_manager =
        this->_network_manager.infinit_instance_manager();
      if (tr.status == plasma::TransactionStatus::finished)
        return 1.0f;
      else if (tr.status != plasma::TransactionStatus::started)
        return 0.0f;
      else if (this->_states[id].state != State::running)
        return 0.0f;
      else if (not instance_manager.exists(tr.network_id))
        return 0.0f;

      auto& progress = this->_progresses(
        [&id] (TransactionProgressMap& map) -> TransactionProgress&
        {
          auto& ptr = map[id];
          if (ptr == nullptr)
            ptr = elle::make_unique<TransactionProgress>();
          return *ptr;
        }
      );

      if (progress.process == nullptr)
      {
        std::string const& progress_binary =
          common::infinit::binary_path("8progress");
        std::list<std::string> arguments{
          "-n", tr.network_id,
          "-u", this->_self.id,
        };

        ELLE_DEBUG("launch: %s %s", progress_binary,
                   boost::algorithm::join(arguments, " "));

        this->_progresses(
          [&] (TransactionProgressMap&)
          {
            progress.process = elle::make_unique<elle::system::Process>(
              binary_check_output_config("8progress", this->_self.id,
                                         tr.network_id),
              progress_binary,
              arguments);
          });
      }

      return this->_progresses(
        [&] () -> float
        {
          if (progress.process == nullptr)
            return 0.0f;
          if (!progress.process->running())
          {
            if (progress.process->status() != 0)
            {
              progress.last_value = 0.0f;
            }
            else
            {
              int current_size = 0;
              int total_size = 0;
              std::stringstream ss;
              ss << progress.process->read();
              ss >> current_size >> total_size;
              if (total_size == 0)
                progress.last_value = 0.0f;
              else
                progress.last_value = float(current_size) / float(total_size);
            }
            progress.process.reset();

            if (progress.last_value < 0)
            {
              ELLE_WARN("8progress returned a negative integer: %s", progress);
              progress.last_value = 0;
            }
            else if (progress.last_value > 1.0f)
            {
              ELLE_WARN("8progress returned an integer greater than 1: %s",
                        progress);
              progress.last_value = 1.0f;
            }
          }
          return progress.last_value;
        }
      );
    }


    void
    TransactionManager::update(std::string const& transaction_id,
                               plasma::TransactionStatus status)
    {
      ELLE_TRACE_METHOD(transaction_id, status);

      ELLE_TRACE("set status %s on transaction %s", status, transaction_id);
      this->_meta.update_transaction(transaction_id, status);
    }

    void
    TransactionManager::accept_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);

      ELLE_ASSERT_EQ(transaction.recipient_id, this->_self.id);
      this->_add<LambdaOperation>(
          "accept_" + transaction.id,
          std::function<void(Operation&)>{
            std::bind(&TransactionManager::_accept_transaction,
                      this,
                      transaction,
                      std::placeholders::_1)});
    }
    void
    TransactionManager::accept_transaction(std::string const& transaction_id)
    {
      ELLE_TRACE_METHOD(transaction_id);

      this->accept_transaction(this->one(transaction_id));
    }

    void
    TransactionManager::_accept_transaction(Transaction const& transaction,
                                            Operation& operation)
    {
      ELLE_TRACE_METHOD(transaction, operation);

      try
      {

        this->_network_manager.add_device(transaction.network_id,
                                          this->_device.id);
        this->_network_manager.prepare(transaction.network_id);
        this->_network_manager.to_directory(
          transaction.network_id,
          common::infinit::network_shelter(this->_self.id,
                                           transaction.network_id));
        this->_network_manager.wait_portal(transaction.network_id);
        this->_meta.accept_transaction(transaction.id,
                                       this->_device.id,
                                       this->_device.name);
      }
      CATCH_FAILURE_TO_METRICS("transaction_accept");

      if (transaction.status == plasma::TransactionStatus::created)
      {
        this->_reporter.store("transaction_accept_preparing",
                              {{MKey::value, transaction.id}});
      }
      else if (transaction.status == plasma::TransactionStatus::started)
      {
        this->_reporter.store("transaction_accept_prepared",
                              {{MKey::value, transaction.id}});
      }

    }

    void
    TransactionManager::cancel_transaction(std::string const& transaction_id)
    {
      ELLE_TRACE_METHOD(transaction_id);

      this->_cancel_transaction(this->one(transaction_id));
    }

    void
    TransactionManager::_cancel_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG_METHOD(transaction);

      this->_add<LambdaOperation>(
        "cancel_" + transaction.id,
        std::function<void()>{
          [&]
          {
            auto scope_exit = [&, transaction]
            {
              this->_cancel_all(transaction.id);
              this->_network_manager.delete_(transaction.network_id, false);
            };
            ELLE_SCOPE_EXIT(scope_exit);

            try
            {
              this->_meta.update_transaction(transaction.id,
                                             plasma::TransactionStatus::canceled);
            }
            CATCH_FAILURE_TO_METRICS("transaction_cancel");

            std::string author = (
              transaction.sender_id == this->_self.id ? "sender" : "recipient");

            auto timestamp_now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch());
            auto timestamp_tr = std::chrono::duration<double>(
              transaction.timestamp);
            double duration = timestamp_now.count() - timestamp_tr.count();

            if (transaction.status == plasma::TransactionStatus::created)
            {
              if (transaction.accepted)
              {
                this->_reporter.store("transaction_cancel_preparing_accepted",
                                      {{MKey::author, author},
                                       {MKey::duration,
                                        std::to_string(duration)},
                                       {MKey::value, transaction.id}});
              }
              else
              {
                this->_reporter.store("transaction_cancel_preparing_unaccepted",
                                      {{MKey::author, author},
                                       {MKey::duration,
                                        std::to_string(duration)},
                                       {MKey::value, transaction.id}});
              }
            }
            else if (transaction.status == plasma::TransactionStatus::started)
            {
              if (transaction.accepted &&
                  this->_user_manager.device_status(
                    transaction.recipient_id,
                    transaction.recipient_device_id))
              {
                this->_reporter.store("transaction_cancel_transferring",
                                      {{MKey::author, author},
                                       {MKey::duration,
                                        std::to_string(duration)},
                                       {MKey::value, transaction.id}});
              }
              else if (transaction.accepted &&
                       !this->_user_manager.device_status(
                         transaction.recipient_id,
                         transaction.recipient_device_id))
              {
                this->_reporter.store("transaction_cancel_offline",
                                      {{MKey::author, author},
                                       {MKey::duration,
                                        std::to_string(duration)},
                                       {MKey::value, transaction.id}});
              }
              else if (!transaction.accepted)
              {
                this->_reporter.store("transaction_cancel_prepared_unaccepted",
                                      {{MKey::author, author},
                                       {MKey::duration,
                                        std::to_string(duration)},
                                       {MKey::value, transaction.id}});
              }
            }
          }
        }
      );
    }

    void
    TransactionManager::_on_cancel_transaction(Transaction const& transaction)
    {
      ELLE_DEBUG_METHOD(transaction);

      std::function<void()> scope_exit = [&, transaction]
      {
        this->_cancel_all(transaction.id);
        this->_network_manager.delete_(transaction.network_id, false);
      };
      this->_add<LambdaOperation>(
        "cancel_" + transaction.id,
        scope_exit
      );
    }

    TransactionManager::TransactionsMap const&
    TransactionManager::all()
    {
      if (this->_transactions->get() != nullptr)
        return *this->_transactions->get();

      this->_transactions([] (TransactionMapPtr& map) {
        if (map == nullptr)
          map.reset(new TransactionsMap{});
      });

      auto response = this->_meta.transactions();
      for (auto const& id: response.transactions)
      {
        auto transaction = this->_meta.transaction(id);
        this->_transactions([&id, &transaction] (TransactionMapPtr& map) {
            (*map)[id] = transaction;
        });
      }

      return *(this->_transactions->get());
    }

    Transaction const&
    TransactionManager::one(std::string const& id)
    {
      auto it = this->all().find(id);
      if (it != this->all().end())
        return it->second;
      return this->sync(id);
    }

    Transaction const&
    TransactionManager::sync(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

      ELLE_TRACE("Synching transaction %s from _meta", id);
      this->all(); // ensure _transactions is not null;
      try
      {
        auto transaction = this->_meta.transaction(id);
        ELLE_DEBUG("Synched transaction %s has status %d",
                   id, transaction.status);
        return this->_transactions(
          [&id, &transaction] (TransactionMapPtr& map)
            -> plasma::Transaction const&
          {
            return (*map)[id] = transaction;
          }
        );
      }
      catch (std::runtime_error const& e)
      {
        throw Exception{gap_transaction_doesnt_exist, e.what()};
      }
    }

    void
    TransactionManager::_clean_transaction(Transaction const& tr)
    {
      ELLE_DEBUG_METHOD(tr);

      ELLE_ASSERT_NEQ(tr.status, plasma::TransactionStatus::created);
      ELLE_ASSERT_NEQ(tr.status, plasma::TransactionStatus::started);
      auto s = this->_states[tr.id];
      if (s.state != State::none)
        try
        {
          this->finalize(s.operation);
        }
        catch (std::exception const&)
        {
          ELLE_DEBUG("couldn't finalize operation: %s",
                     elle::exception_string());
        }

      this->_states->erase(tr.id);
      this->_cancel_all(tr.id);
      // Only delete local data of successful transfers
      this->_network_manager.delete_(
        tr.network_id,
        tr.status == plasma::TransactionStatus::finished);
    }

    void
    TransactionManager::_on_transaction(plasma::Transaction const& tr)
    {
      ELLE_DEBUG_METHOD(tr);

      ELLE_DEBUG("received transaction %s, update local copy", tr)
      {
        // Ensure map is not null
        this->all();
        this->_transactions([&tr] (TransactionMapPtr& ptr) {
            (*ptr)[tr.id] = tr;
        });
      }
      if (tr.status == plasma::TransactionStatus::canceled)
      {
        this->_on_cancel_transaction(tr);
        return;
      }
      else if (tr.status != plasma::TransactionStatus::created and
          tr.status != plasma::TransactionStatus::started)
      {
        ELLE_DEBUG("Cleaning up finished transaction %s", tr);
        this->_clean_transaction(tr);
        return;
      }

      if (tr.sender_id == this->_self.id)
      {
        if (tr.sender_device_id != this->_device.id)
        {
          // XXX
          // for now, all the user's devices receive notifications; trophonius
          // needs to be improved so as to send notifications to devices rather
          // than to users, at least for some types of notification

          // ELLE_ASSERT(
          //     false,
          //     "got a transaction tr that does not involve my device: %s",
          //     tr);
          ELLE_WARN("XXX Should be an assert: got device unrelated tr");
          return;
        }
        if (tr.status == plasma::TransactionStatus::created)
        {
          ELLE_DEBUG("sender prepare upload for %s", tr)
            this->_prepare_upload(tr);
        }
        else if (tr.status == plasma::TransactionStatus::started &&
                 tr.accepted &&
                 this->_user_manager.device_status(tr.recipient_id,
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
          // ELLE_ASSERT(
          //     false,
          //     "got a transaction tr that does not involve my device: %s",
          //     tr);
          ELLE_WARN("XXX Should be an assert: got device unrelated tr");
          return;
        }
        if (tr.status == plasma::TransactionStatus::started &&
            tr.accepted &&
            this->_user_manager.device_status(tr.sender_id,
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
          else if (!this->_user_manager.device_status(tr.sender_id,
                                                      tr.sender_device_id))
            reason = "sender device_id is down";
          ELLE_DEBUG("recipient does nothing for %s (%s)", tr, reason);
#endif
        }
      }
      else
      {
        ELLE_WARN("got a transaction tr not related to me: %s", tr);
        return;
      }
    }

    void
    TransactionManager::_on_user_status(UserStatusNotification const& notif)
    {
      ELLE_DEBUG_METHOD(notif);

      // Search for user related transactions.
      auto to_check = this->_transactions(
        [&] (TransactionMapPtr& map_ptr) -> std::vector<plasma::Transaction>
        {
          std::vector<plasma::Transaction> found;
          if (map_ptr != nullptr)
            for (auto const& pair: *map_ptr)
              if (pair.second.recipient_id == notif.user_id or
                  pair.second.sender_id == notif.user_id)
                found.push_back(pair.second);
          return found;
        });
      // Check again if something should happen.
      for (auto const& tr: to_check)
        this->_on_transaction(tr);
    }

    void
    TransactionManager::_prepare_upload(Transaction const& tr)
    {
      ELLE_DEBUG_METHOD(tr);

      auto s = this->_states[tr.id];

      // XXX ghost user doesn't have public key so check this before adding to
      // network.
      if (s.state == State::none and
          not this->_user_manager.one(tr.recipient_id).public_key.empty())
      {
        ELLE_DEBUG("prepare transaction %s", tr)
        this->_reporter.store("transaction_preparing",
                              {{MKey::count, std::to_string(tr.files_count)},
                               {MKey::network, tr.network_id},
                               {MKey::size, std::to_string(tr.total_size)},
                               {MKey::value, tr.id}});
        s.operation = this->_add<PrepareTransactionOperation>(
          *this,
          this->_network_manager,
          this->_meta,
          this->_reporter,
          this->_self,
          tr,
          s.files);
        s.state = State::preparing;
        this->_states(
          [&tr, &s] (StateMap& map) {map[tr.id] = s;});
      }
      else
      {
        if (s.state == State::preparing)
        {
          ELLE_DEBUG("do not prepare %s, already in state %d",
                     tr,
                     s.state);
        }

        else if (this->_user_manager.one(tr.recipient_id).public_key.empty())
        {
          ELLE_DEBUG("do not prepare %s, recipient hasn't registered yet",
                     tr);
        }
      }
    }

    void
    TransactionManager::_start_upload(Transaction const& transaction)
    {
      ELLE_DEBUG_METHOD(transaction);

      auto s = this->_states[transaction.id];

      // if (s.tries == 1) //XXX variable for that
      // {
      //   this->_meta.update_transaction(transaction.id,
      //                                  plasma::TransactionStatus::failed);
      //   this->_states->erase(transaction.id);
      //   auto timestamp_now = std::chrono::duration_cast<std::chrono::seconds>(
      //     std::chrono::system_clock::now().time_since_epoch());
      //   auto timestamp_tr = std::chrono::duration<double>(transaction.timestamp);
      //   double duration = timestamp_now.count() - timestamp_tr.count();
      //   this->_reporter.store("transaction_transferring_fail",
      //                         {{MKey::attempt, std::to_string(s.tries)},
      //                          {MKey::duration, std::to_string(duration)},
      //                          {MKey::network,transaction.network_id},
      //                          {MKey::value, transaction.id}});
      //   return;
      // }

      // // If the transaction is running, cancel it.
      // if (s.state == State::running)
      // {
      //   if (this->status(s.operation) == OperationStatus::running)
      //   {
      //     ELLE_LOG("transfer %s had an error, restarting", transaction.id);
      //     this->cancel_operation(s.operation);
      //   }
      //   s.state = State::preparing;
      //   s.operation = 0;
      // }

      if (s.state == State::preparing)
      {
        s.operation = this->_add<UploadOperation>(
          transaction.id,
          this->_network_manager,
          *this,
          this->_network_manager.infinit_instance_manager(),
          [this, transaction] {
            this->_network_manager.notify_8infinit(
              transaction.network_id,
              transaction.sender_device_id,
              transaction.recipient_device_id);
            });
        s.state = State::running;
        s.tries += 1;
        this->_states(
          [&transaction, &s] (StateMap& map) {map[transaction.id] = s;});
        this->_reporter.store("transaction_transferring",
                              {{MKey::attempt, std::to_string(s.tries)},
                               {MKey::network,transaction.network_id},
                               {MKey::value, transaction.id}});
      }
      else
      {
        if (s.state != State::preparing)
          ELLE_DEBUG("cannot start upload of %s, state is not preparing: %s",
                     transaction, (int) s.state);
        else if (this->status(s.operation) == OperationStatus::failure)
          ELLE_DEBUG("cannot start upload of %s, prepare failed", transaction);
        else if (this->status(s.operation) == OperationStatus::running)
          ELLE_DEBUG("cannot start upload of %s, prepare still running",
                     transaction);
        else
          ELLE_DEBUG("XXX cannot start upload (should not be printed)");
      }
    }

    void
    TransactionManager::_start_download(Transaction const& transaction)
    {
      ELLE_DEBUG_METHOD(transaction);

      auto state = this->_states[transaction.id];
      // if (state.state == State::running)
      // {
      //   ELLE_LOG("transfer %s met an error, restarting", transaction.id);
      //   this->cancel_operation(state.operation);
      //   this->_network_manager.delete_local(transaction.network_id);
      //   this->_network_manager.prepare(transaction.network_id);
      //   this->_network_manager.to_directory(
      //     transaction.network_id,
      //     common::infinit::network_shelter(this->_self.id,
      //                                      transaction.network_id));
      //   this->_network_manager.wait_portal(transaction.network_id);
      //   state.state = State::none;
      // }

      if (state.state == State::none)
      {
        state.operation = this->_add<DownloadOperation>(
            *this,
            this->_network_manager,
            this->_self,
            this->_reporter,
            transaction,
            std::bind(&NetworkManager::notify_8infinit,
                      &(this->_network_manager),
                      transaction.network_id,
                      transaction.sender_device_id,
                      transaction.recipient_device_id));
        state.state = State::running;
        state.tries += 1;
        this->_states(
          [&transaction, &state] (StateMap& map) {map[transaction.id] = state;});
      }
      else
      {
        if (state.state != State::none)
          ELLE_TRACE("cannot start download of %s, state is not none: %s",
                     transaction, (int) state.state);
        else
          ELLE_TRACE("XXX cannot start upload (should not be printed)");
      }
    }
  }
}
