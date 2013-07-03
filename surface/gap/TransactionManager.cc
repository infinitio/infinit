#include "TransactionManager.hh"

#include "binary_config.hh"
#include "CreateTransactionOperation.hh"
#include "DownloadOperation.hh"
#include "PrepareTransactionOperation.hh"
#include "UploadOperation.hh"
#include "metrics.hh"

#include <plasma/meta/Client.hh>

#include <common/common.hh>

#include <elle/assert.hh>
#include <elle/os/path.hh>
#include <elle/os/file.hh>
#include <elle/os/getenv.hh>
#include <elle/container/set.hh>
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

      auto total_size =
        [] (std::unordered_set<std::string> const& files) -> size_t
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
      };

      int size = total_size(files);

      std::string first_file = fs::path(*(files.cbegin())).filename().string();

      elle::utility::Time time; time.Current();
      std::string network_name = elle::sprintf("%s-%s",
                                               recipient_id_or_email,
                                               time.nanoseconds);

      std::string network_id = this->_network_manager.create(network_name);
      // XXX add locally

      // Preparing the network before sending the notification ensures that the
      // recipient can't prepare it by himself.
      this->_network_manager.prepare(network_id);
      this->_network_manager.to_directory(
        network_id,
        common::infinit::network_shelter(this->_self.id, network_id));

      plasma::meta::CreateTransactionResponse res;
      ELLE_DEBUG("Send %s (%sB) to %s via network %s",
                 first_file, size, recipient_id_or_email, network_id);

      std::string transaction_id = "";
      try
      {
        res = this->_meta.create_transaction(recipient_id_or_email,
                                             first_file,
                                             files.size(),
                                             size,
                                             fs::is_directory(first_file),
                                             network_id,
                                             this->_device.id);

        transaction_id = res.created_transaction_id;

        auto s = this->_states[transaction_id];
        s.files = files;
        this->_states(
          [&transaction_id, &s] (StateMap& map) {map[transaction_id] = s;});

        // Creating a transaction ensures that user has an id.
        auto recipient = this->_user_manager.one(recipient_id_or_email);
        ELLE_TRACE("add user %s to network %s", recipient, network_id)
          this->_meta.network_add_user(network_id, recipient.id);
      }
      catch (...)
      {
        ELLE_DEBUG("transaction creation failed: %s", elle::exception_string());
        if (!transaction_id.empty())
          this->cancel_transaction(transaction_id);
        else
          this->_network_manager.delete_(network_id, false);
        throw;
      }
      this->_self.remaining_invitations = res.remaining_invitations;

      return 0;
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
      else if (!instance_manager.exists(tr.network_id))
        return 0.0f;

      return this->_network_manager.progress(tr.network_id);
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
        this->_network_manager.launch(transaction.network_id);
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
          ELLE_ERR(
            "got a transaction %s that does not involve my device", tr);
          ELLE_ASSERT(false && "invalid transaction_id");
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
          if (!tr.recipient_device_id.empty())
          {
            ELLE_ERR(
              "got an accepted transaction that does not involve my device: %s",
              tr);
            ELLE_ASSERT(false && "invalid transaction_id");
            return;
          }
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
        ELLE_ERR("got a transaction not related to me:", tr);
        ELLE_ASSERT(false && "invalid transaction_id");
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

    // XXX[Antony]: This remove the asynchronousity =)
    void
    TransactionManager::_prepare_upload(Transaction const& tr)
    {
      auto s = this->_states[tr.id];

      ELLE_TRACE_SCOPE("%s: uploading files %s for transaction %s, network %s",
                       *this, s.files, tr.id, tr.network_id);

      ELLE_DEBUG("%s: state is %s", *this, s.state);
      ELLE_DEBUG("%s: peer public key %s", *this, this->_user_manager.one(tr.recipient_id).public_key);

      if (s.state == State::none and
          not this->_user_manager.one(tr.recipient_id).public_key.empty())
      {
        this->_network_manager.prepare(tr.network_id);
        this->_network_manager.to_directory(
        tr.network_id,
        common::infinit::network_shelter(this->_self.id,
                                         tr.network_id));

        this->_network_manager.launch(tr.network_id);

        std::string recipient_K =
          this->_meta.user(tr.recipient_id).public_key;
        ELLE_ASSERT_NEQ(recipient_K.size(), 0u);

        this->_network_manager.add_user(tr.network_id, recipient_K);
        this->_network_manager.set_permissions(tr.network_id, recipient_K);

        s.state = State::preparing;
        this->_states([&tr, &s] (StateMap& map) {map[tr.id] = s;});

        elle::metrics::Reporter& reporter = this->_reporter;

        this->_network_manager.upload_files(
          tr.network_id,
          s.files,
          [&reporter, tr, this]
          {
            reporter.store(
              "transaction_prepared",
              {{MKey::value, tr.id},
                {MKey::network, tr.network_id},
                {MKey::count, std::to_string(tr.files_count)},
                {MKey::size, std::to_string(tr.total_size)}});

            this->update(tr.id,
                         plasma::TransactionStatus::started);

          },
          [&reporter, tr, this]
          {
            reporter.store(
              "transaction_preparing_failed",
              {{MKey::value, tr.id},
                {MKey::network, tr.network_id},
                {MKey::count, std::to_string(tr.files_count)},
                {MKey::size, std::to_string(tr.total_size)}});

            this->update(tr.id,
                         plasma::TransactionStatus::failed);

          });

        ELLE_DEBUG("%s: finished preparing %s locally for network %s",
                   *this, s.files, tr.network_id);
      }
    }

    void
    TransactionManager::_start_upload(Transaction const& transaction)
    {
      ELLE_DEBUG_METHOD(transaction);

      auto s = this->_states[transaction.id];

      if (s.state == State::preparing)
      {
        this->_network_manager.notify_8infinit(transaction.network_id,
                                               transaction.sender_device_id,
                                               transaction.recipient_device_id);

        s.state = State::running;
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

      if (state.state == State::none)
      {
        this->_network_manager.notify_8infinit(transaction.network_id,
                                               transaction.sender_device_id,
                                               transaction.recipient_device_id);

        elle::metrics::Reporter& reporter = this->_reporter;

        this->_network_manager.download_files(
          transaction.network_id,
          this->_self.public_key,
          this->_output_dir,
          [&reporter, transaction, this]
          {
            auto timestamp_now =
              std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch());
            auto timestamp_tr = std::chrono::duration<double>(
              transaction.timestamp);
            double duration = timestamp_now.count() - timestamp_tr.count();
            reporter.store(
              "transaction_transferred",
              {{MKey::duration, std::to_string(duration)},
                {MKey::value, transaction.id},
                {MKey::network, transaction.network_id},
                {MKey::count, std::to_string(transaction.files_count)},
                {MKey::size, std::to_string(transaction.total_size)}});

            this->update(transaction.id, plasma::TransactionStatus::finished);
          },
          [&reporter, transaction, this]
          {
            reporter.store(
              "transaction_transferring_fail",
              {{MKey::value, transaction.id},
                {MKey::network, transaction.network_id},
                {MKey::count, std::to_string(transaction.files_count)},
                {MKey::size, std::to_string(transaction.total_size)}});

            this->update(transaction.id, plasma::TransactionStatus::failed);
          });
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
