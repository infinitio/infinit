#include "TransactionManager.hh"

#include "binary_config.hh"
#include "metrics.hh"

#include <metrics/Reporter.hh>

#include <plasma/meta/Client.hh>

#include <common/common.hh>

#include <reactor/exception.hh>

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

ELLE_LOG_COMPONENT("infinit.surface.gap.TransactionManager");

namespace surface
{
  namespace gap
  {
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

    TransactionManager::TransactionManager(
      reactor::Scheduler& scheduler,
        NotificationManager& notification_manager,
        NetworkManager& network_manager,
        UserManager& user_manager,
        plasma::meta::Client& meta,
        metrics::Reporter& reporter,
        SelfGetter const& self,
        DeviceGetter const& device,
        UpdateRemainingInvitations const& update_remaining_invitations):
      Notifiable(notification_manager),
      _network_manager(network_manager),
      _user_manager(user_manager),
      _meta(meta),
      _reporter(reporter),
      _scheduler(scheduler),
      _self{self},
      _device{device},
      _update_remaining_invitations{update_remaining_invitations},
      _output_dir{common::system::download_directory()},
      _state_machine{
        std::bind(&TransactionManager::_on_cancel_transaction,
                  this,
                  std::placeholders::_1),
        std::bind(&TransactionManager::_on_failed_transaction,
                  this,
                  std::placeholders::_1),
        std::bind(&TransactionManager::_clean_transaction,
                  this,
                  std::placeholders::_1),
        std::bind(&TransactionManager::_prepare_upload,
                  this,
                  std::placeholders::_1),
        std::bind(&TransactionManager::_start_upload,
                  this,
                  std::placeholders::_1),
        std::bind(&TransactionManager::_start_download,
                  this,
                  std::placeholders::_1),
        std::bind(&UserManager::device_status,
                  &_user_manager,
                  std::placeholders::_1,
                  std::placeholders::_2),
        self,
        device,
      }
    {
      ELLE_ASSERT(this->_self != nullptr);
      ELLE_ASSERT(this->_device != nullptr);
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
      catch (std::runtime_error const&)
      {
        ELLE_WARN("couldn't clear the transaction manager: %s",
                  elle::exception_string());
      }
      ELLE_TRACE("%s: ~TransactionManager() exited", *this);
    }

    void
    TransactionManager::_on_failed_transaction(Transaction const& tr)
    {
      ELLE_DEBUG("failed transaction(%s) with network(%s) for user(%s)",
                 tr.id, tr.network_id, this->_self().id);
      //gap_gather_crash_reports(this->self()._id, tr.network_id);
    }

    void
    TransactionManager::clear()
    {
      this->_network_manager.clear();
    }

    void
    TransactionManager::output_dir(std::string const& dir)
    {
      if (!fs::exists(dir))
        throw Exception{gap_error,
                        "directory doesn't exist."};

      if (!fs::is_directory(dir))
        throw Exception{gap_error,
                        "not a directroy."};

      this->_output_dir = dir;
    }

    void
    TransactionManager::send_files(std::string const& recipient_id_or_email,
                                   std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_SCOPE("%s: send %s to %s",
                       *this, files, recipient_id_or_email);

      if (files.empty())
        throw Exception(gap_no_file, "no files to send");

      new reactor::Thread(
        this->_scheduler,
        elle::sprintf("send files %s", recipient_id_or_email),
        [this, recipient_id_or_email, files]
        {
          try
          {
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
            bool destroy_locally = false;
            elle::Finally network_guard(
              [this, network_id, &destroy_locally] ()
              {
                this->_network_manager.delete_(network_id, destroy_locally);
              }
              );

            // Preparing the network before sending the notification ensures that the
            // recipient can't prepare it by himself.
            this->_network_manager.prepare(network_id);
            this->_network_manager.to_directory(
              network_id,
              common::infinit::network_shelter(this->_self().id, network_id));

            destroy_locally = true;

            plasma::meta::CreateTransactionResponse res;
            ELLE_DEBUG("Send %s (%sB) to %s via network %s",
                       first_file, size, recipient_id_or_email, network_id);

            std::string transaction_id = "";
             res = this->_meta.create_transaction(recipient_id_or_email,
                                                 first_file,
                                                 files.size(),
                                                 size,
                                                 fs::is_directory(first_file),
                                                 network_id,
                                                 this->_device().id);
            transaction_id = res.created_transaction_id;

            elle::Finally transaction_guard(
              [this, transaction_id]
              {
                this->cancel_transaction(transaction_id);
              }
            );


            auto s = this->_states[transaction_id];
            s.files = files;
            this->_states(
              [&transaction_id, &s] (StateMap& map) {map[transaction_id] = s;});

            // Creating a transaction ensures that user has an id.
            auto recipient = this->_user_manager.one(recipient_id_or_email);
            ELLE_TRACE("add user %s to network %s", recipient, network_id)
              this->_meta.network_add_user(network_id, recipient.id);

            network_guard.abort();
            transaction_guard.abort();

            this->_update_remaining_invitations(res.remaining_invitations);
          }
          catch (reactor::Terminate const&)
          {
            throw;
          }
          catch (...)
          {
            ELLE_DEBUG("transaction creation failed: %s",
                       elle::exception_string());
          }
        },
        true);
    }

    float
    TransactionManager::progress(std::string const& id)
    {
      ELLE_TRACE_SCOPE("%s: get progress for %s", *this, id);

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
    TransactionManager::_update(std::string const& transaction_id,
                               plasma::TransactionStatus status)
    {
      ELLE_TRACE_SCOPE("%s: Update transaction %s with status %s",
                       *this, transaction_id, status);

      ELLE_TRACE("set status %s on transaction %s", status, transaction_id);
      this->_meta.update_transaction(transaction_id, status);
    }

    void
    TransactionManager::accept_transaction(std::string const& transaction_id)
    {
      this->accept_transaction(this->one(transaction_id));
    }

    void
    TransactionManager::accept_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: accept %s", *this, transaction);
      ELLE_ASSERT_EQ(transaction.recipient_id, this->_self().id);

      if (transaction.accepted == true)
        throw elle::Exception(
          elle::sprintf("transaction %s is already accepted.", transaction));

      auto s = this->_states[transaction.id];

      if (s.state != State::none && s.state != State::accepting)
        throw elle::Exception(
          elle::sprintf("transaction %s has with a bad local state %s",
                        s.state));

      if (s.state == State::none)
      {
        ELLE_DEBUG("%s: change local state %s to accepted", transaction, s);
        s.state = State::accepting;
        this->_states([&transaction, &s] (StateMap& map) {map[transaction.id] = s;});


        this->_network_manager.add_device(transaction.network_id,
                                          this->_device().id);
        this->_network_manager.prepare(transaction.network_id);
        this->_network_manager.to_directory(
          transaction.network_id,
          common::infinit::network_shelter(this->_self().id,
                                           transaction.network_id));
        this->_network_manager.launch(transaction.network_id);

        // Long.
        this->_meta.accept_transaction(transaction.id,
                                       this->_device().id,
                                       this->_device().name);
        if (transaction.status == plasma::TransactionStatus::created)
        {
          this->_reporter[transaction.id].store(
            "transaction.preparing.accepted",
            {{MKey::value, transaction.id}});
        }
      }
      else
      {
        // meta.accept_transaction is done at the end of the previous block,
        // and many operations occure before. Some can be long and the http
        // request to meta too.
        // So there is a time laps when you localy accepted the transaction but
        // the transaction.accepted is still false. This log may seem awkward
        // but it's not, especialy if you didn't lock the accepting process on
        // top level (mac app, python script, ...).
        ELLE_DEBUG("%s: Accepting the already accepted transaction %s",
                   *this, transaction);
      }
    }

    void
    TransactionManager::cancel_transaction(std::string const& transaction_id)
    {
      this->_cancel_transaction(this->one(transaction_id));
    }

    metrics::Metric
    transaction_metric(Self const& self,
                       UserManager& user_manager,
                       Transaction const& tr)
    {
      std::string author = (
        tr.sender_id == self.id ? "sender" : "recipient");

      auto timestamp_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
      auto timestamp_tr = std::chrono::duration<double>(tr.timestamp);
      double duration = timestamp_now.count() - timestamp_tr.count();

      return metrics::Metric{
        {MKey::author, author},
        {MKey::duration, std::to_string(duration)},
        {MKey::count, std::to_string(tr.files_count)},
        {MKey::size, std::to_string(tr.total_size)},
        {MKey::network, tr.network_id},
        {MKey::value, tr.id},
        {
          MKey::sender_online,
          user_manager.device_status(tr.sender_id,
                                     tr.sender_device_id) ?
            "true" :
            "false"
        },
        {
          MKey::recipient_online,
          user_manager.device_status(tr.recipient_id,
                                     tr.recipient_device_id) ?
            "true" :
            "false"
        },
      };
    }

    void
    TransactionManager::_cancel_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: cancel %s", *this, transaction);
      this->_states(
        [&transaction] (StateMap& map) {
          map[transaction.id].state = State::canceled;
        });
      elle::Finally scope_exit{
        [&, transaction] {
          // XXX why not delete local files ?
          try
          {
            this->_network_manager.delete_(transaction.network_id, false);
          }
          catch (elle::Exception const& e)
          {
            ELLE_ERR("failed to delete network(%s): %s",
                     transaction.network_id, elle::exception_string());
          }
        }};


      auto& reporter = this->_reporter[transaction.id];

      auto metric = transaction_metric(this->_self(),
                                       this->_user_manager,
                                       transaction);
      if (transaction.status == plasma::TransactionStatus::created)
      {
        if (transaction.accepted)
          reporter.store("transaction.preparing.accepted.canceled", metric);
        else
          reporter.store("transaction.preparing.canceled", metric);
      }
      else if (transaction.status == plasma::TransactionStatus::started)
      {
        if (transaction.accepted)
          reporter.store("transaction.transfering.canceled", metric);
        else
          reporter.store("transaction.prepared.canceled", metric);
      }
      else
        reporter.store("transaction.unknown.canceled", metric);

      this->_meta.update_transaction(transaction.id,
                                     plasma::TransactionStatus::canceled);
    }

    void
    TransactionManager::_on_cancel_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: cancel callback for %s", *this, transaction);

      this->_states(
        [&transaction] (StateMap& map) {
          map[transaction.id].state = State::canceled;
        });

      /// XXX: False means that peer of the deleter will never remo
      this->_network_manager.delete_(transaction.network_id, false);
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
      ELLE_TRACE_SCOPE("%s: sync transaction %s from meta", *this, id);
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

      this->_states->erase(tr.id);
      // Only delete local data of successful transfers
      this->_network_manager.delete_(
        tr.network_id, tr.status == plasma::TransactionStatus::finished);
    }

    void
    TransactionManager::_on_transaction(Transaction const& tr)
    {
      ELLE_TRACE_SCOPE("%s: transaction callback for %s", *this, tr);

      ELLE_ASSERT(tr.recipient_id == this->_self().id ||
                  tr.sender_id == this->_self().id);

      if (tr.sender_device_id != this->_device().id &&
          (tr.recipient_device_id != this->_device().id &&
           not tr.recipient_device_id.empty()))
      {
        ELLE_TRACE("ignore transaction %s: not related to my device", tr);
        return;
      }

      ELLE_DEBUG("received transaction %s, update local copy", tr)
      {
        // Ensure map is not null
        this->all();
        this->_transactions([&tr] (TransactionMapPtr& ptr) {
            auto it = ptr->find(tr.id);
            if (it != ptr->end())
              if (it->second.status > tr.status)
              {
                throw elle::Exception{
                  elle::sprint(
                    "ignore transaction", tr,
                    "because local status of", it->second,
                    "is greater"
                  )};
                return;
              }
            (*ptr)[tr.id] = tr;
        });
      }

      this->_state_machine(tr);
    }

    void
    TransactionManager::_on_user_status(UserStatusNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: user status callback for %s", *this, notif);

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

      ELLE_TRACE_SCOPE("%s: upload files %s for transaction %s, network %s",
                       *this, s.files, tr.id, tr.network_id);

      ELLE_DEBUG("%s: state is %s", *this, s.state);
      ELLE_DEBUG("%s: peer public key %s", *this, this->_user_manager.one(tr.recipient_id).public_key);

      if (s.state == State::none and
          not this->_user_manager.one(tr.recipient_id).public_key.empty())
      {

        this->_reporter[tr.id].store(
          "transaction.preparing",
          transaction_metric(this->_self(), this->_user_manager, tr));
        this->_network_manager.prepare(tr.network_id);
        this->_network_manager.to_directory(
        tr.network_id,
        common::infinit::network_shelter(this->_self().id,
                                         tr.network_id));

        this->_network_manager.launch(tr.network_id);

        std::string recipient_K =
          this->_meta.user(tr.recipient_id).public_key;
        ELLE_ASSERT_NEQ(recipient_K.size(), 0u);

        this->_network_manager.add_user(tr.network_id, recipient_K);
        this->_network_manager.set_permissions(tr.network_id, recipient_K);

        ELLE_DEBUG("%s: change local state %s to preparing", tr, s);
        s.state = State::preparing;
        this->_states([&tr, &s] (StateMap& map) {map[tr.id] = s;});

        auto& reporter = this->_reporter;

        this->_network_manager.upload_files(
          tr.network_id,
          s.files,
          [&reporter, tr, this]
          {
            try
            {
              this->_update(tr.id, plasma::TransactionStatus::started);

              reporter[tr.id].store(
                "transaction.prepared",
                transaction_metric(this->_self(), this->_user_manager, tr));
            }
            catch (plasma::meta::Exception const& e)
            {
              if (e != plasma::meta::Error::transaction_operation_not_permitted)
                throw;
            }
          },
          [&reporter, tr, this]
          {
            // If the transaction was already marked as cancelled, do not mark
            // it as failed.
            auto s = this->_states[tr.id];
            if (s.state == State::canceled)
              return;

            try
            {
              reporter[tr.id].store(
                "transaction.preparing.failed",
                transaction_metric(this->_self(), this->_user_manager, tr));

              this->_update(tr.id, plasma::TransactionStatus::failed);
            }
            catch (plasma::meta::Exception const& e)
            {
              if (e != plasma::meta::Error::transaction_operation_not_permitted)
                throw;
            }
          });

        ELLE_DEBUG("%s: finished preparing %s locally for network %s",
                   *this, s.files, tr.network_id);
      }
    }

    void
    TransactionManager::_start_upload(Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: start upload for %s", *this, transaction);

      auto s = this->_states[transaction.id];

      if (s.state != State::running && s.state != State::finished)
      {
        ELLE_DEBUG("%s: change local state %s to running", transaction, s);
        s.state = State::running;
        this->_states(
          [&transaction, &s] (StateMap& map) {map[transaction.id] = s;});

        // XXX.
        this->_network_manager.ensure_launched(transaction.network_id);
        // The progress will wait for the start_progress signal, so it's
        // required to start it before the connect_try.
        this->_network_manager.infinit_instance_manager().run_progress(
          transaction.network_id);

        this->_network_manager.infinit_instance_manager().connect_try(
          transaction.network_id,
          this->_network_manager.peer_addresses(transaction.network_id,
                                                transaction.sender_device_id,
                                                transaction.recipient_device_id),
          false);

        this->_network_manager.infinit_instance_manager().run_progress(
          transaction.network_id);

        this->_reporter[transaction.id].store(
          "transaction.transfering",
          transaction_metric(this->_self(), this->_user_manager, transaction));
      }
      else
      {
        if (s.state != State::preparing)
          ELLE_DEBUG("cannot start upload of %s, state is not preparing: %s",
                     transaction, (int) s.state);
        else
          ELLE_DEBUG("XXX cannot start upload (should not be printed)");
      }
    }

    void
    TransactionManager::_start_download(Transaction const& transaction)
    {
      ELLE_TRACE_SCOPE("%s: start download for %s", *this, transaction);

      auto state = this->_states[transaction.id];

      if (state.state != State::finished && state.state != State::running)
      {
        ELLE_DEBUG("%s: change local state %s to running", transaction, state);
        state.state = State::running;
        this->_states(
          [&transaction, &state] (StateMap& map) {map[transaction.id] = state;});

        auto& reporter = this->_reporter;

        // XXX
        this->_network_manager.ensure_launched(transaction.network_id);

        this->_network_manager.download_files(
          transaction.network_id,
          this->_network_manager.peer_addresses(transaction.network_id,
                                                transaction.sender_device_id,
                                                transaction.recipient_device_id),
          this->_self().public_key,
          this->_output_dir,
          [&reporter, transaction, this]
          {
            try
            {
              auto& imap =
                this->_network_manager.infinit_instance_manager().instances();
              auto& instance = imap[transaction.network_id];
              this->_update(transaction.id,
                            plasma::TransactionStatus::finished);

              auto metric =
                transaction_metric(this->_self(),
                                   this->_user_manager,
                                   transaction);
              if (instance->forwarder)
                metric[MKey::method] = 2;
              reporter[transaction.id].store("transaction.transfered", metric);
            }
            catch (plasma::meta::Exception const& e)
            {
              if (e != plasma::meta::Error::transaction_operation_not_permitted)
                throw;
            }

            auto state = this->_states[transaction.id];
            ELLE_DEBUG("%s: change local state %s to finished",
                       transaction, state);
            state.state = State::finished;
            this->_states(
              [&transaction, &state] (StateMap& map)
              {
                map[transaction.id] = state;
              }
            );
          },
          [&reporter, transaction, this]
          {
            // If the transaction was already marked as cancelled, do not mark
            // it as failed.
            auto s = this->_states[transaction.id];
            if (s.state == State::canceled)
              return;

            try
            {
              reporter[transaction.id].store(
                "transaction.transfering.failed",
                transaction_metric(this->_self(), this->_user_manager, transaction));

              this->_update(transaction.id, plasma::TransactionStatus::failed);
            }
            catch (plasma::meta::Exception const& e)
            {
              if (e != plasma::meta::Error::transaction_operation_not_permitted)
                throw;
            }
          });
      }
      else
      {
        if (state.state != State::none)
          ELLE_TRACE("cannot start download of %s, state is not none: %s",
                     transaction, state);
        else
          ELLE_TRACE("XXX cannot start upload (should not be printed)");
      }
    }
  }
}
