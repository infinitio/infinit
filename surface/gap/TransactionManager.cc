#include "TransactionManager.hh"

#include "DownloadOperation.hh"
#include "UploadOperation.hh"
#include "metrics.hh"

#include <common/common.hh>

#include <elle/os/path.hh>
#include <elle/os/file.hh>
#include <elle/os/getenv.hh>
#include <elle/system/Process.hh>
#include <elle/memory.hh>
#include <elle/finally.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

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
        [&] (TransactionNotification const &n, bool is_new) -> void
        {
          this->_on_transaction(n, is_new);
        }
      );
    }

    TransactionManager::~TransactionManager()
    {
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

    OperationManager::OperationId
    TransactionManager::send_files(std::string const& recipient_id_or_email,
                                   std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_METHOD(recipient_id_or_email, files);

      if (files.empty())
        throw Exception(gap_no_file, "no files to send");

      return this->_add<UploadOperation>(*this,
                                         this->_network_manager,
                                         this->_meta,
                                         this->_reporter,
                                         this->_self,
                                         this->_device.id,
                                         recipient_id_or_email,
                                         files);
    }

    float
    TransactionManager::progress(std::string const& id)
    {
      ELLE_TRACE("Retrieve progress of transaction %s", id);

      auto const& tr = this->one(id);
      auto const& instance_manager =
        this->_network_manager.infinit_instance_manager();

      if (tr.status == gap_transaction_status_finished)
        return 1.0f;
      else if (tr.status == gap_transaction_status_canceled)
        return 0.0f;
      else if (tr.status != gap_transaction_status_started)
        return 0.0f;
      else if (!instance_manager.exists(tr.network_id))
      {
        throw Exception{
          gap_network_not_found,
          "Cannot launch 8progress without infinit instance",
        };
      }


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
        this->_progresses(
          [&] (TransactionProgressMap&)
          {
            progress.process = elle::make_unique<elle::system::Process>(
              std::move(pc),
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
            int current_size = 0;
            int total_size = 0;
            std::stringstream ss;
            ss << progress.process->read();
            ss >> current_size >> total_size;
            progress.process.reset();

            if (total_size == 0)
              return 0.f;
            progress.last_value = float(current_size) / float(total_size);

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

    TransactionManager::OperationId
    TransactionManager::_download_files(std::string const& id)
    {
      auto pair = this->all().find(id);

      ELLE_ASSERT(pair != this->all().end());

      if (pair == this->all().end())
      {
        throw Exception(gap_error,
                        elle::sprintf("download failure cause %s doesn't exist",
                                      id));
      }

      Transaction const& trans = pair->second;

      return this->_add<DownloadOperation>(*this,
                                           this->_self,
                                           trans);
    }

    void
    TransactionManager::update(std::string const& id,
                               gap_TransactionStatus status)
    {
      ELLE_TRACE_METHOD(id, status);

      auto const& transaction = this->sync(id); // XXX: remove sync

      ELLE_DEBUG("transaction: %s", transaction);

      typedef void (TransactionManager::*callback_t)(Transaction const&);

      static callback_t callbacks[gap_transaction_status__count] = {0};
      if (callbacks[gap_transaction_status_created] == nullptr)
      {
        callbacks[gap_transaction_status_created] = &TransactionManager::_create_transaction;
        callbacks[gap_transaction_status_started] = &TransactionManager::_start_transaction;
        callbacks[gap_transaction_status_canceled] = &TransactionManager::_cancel_transaction;
        //callbacks[gap_transaction_status_finished] = &TransactionManager::_finish_transaction;
      }

      if (status < 0 ||
          status >= gap_transaction_status__count ||
          callbacks[status] == nullptr)
      {
          ELLE_WARN("invalid status %s", status);
          throw Exception(gap_api_error,
                          elle::sprintf("unknown status %s", status));

      }
      else
      {
        (this->*callbacks[status])(transaction);
      }
    }

    void
    TransactionManager::_ensure_ownership(Transaction const& transaction,
                                          bool check_devices)
    {
      ELLE_ASSERT(this->_self.id == transaction.sender_id ||
                  this->_self.id == transaction.recipient_id);

      if (check_devices)
      {
        ELLE_ASSERT(this->_device.id == transaction.sender_device_id ||
                    this->_device.id == transaction.recipient_device_id);
      }
    }

    void
    TransactionManager::_create_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.sender_device_id, this->_device.id);

      this->_meta.update_transaction(transaction.id,
                                     plasma::TransactionStatus::created);
    }

    void
    TransactionManager::_on_transaction_created(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);
      if (transaction.sender_device_id != this->_device.id)
      {
        ELLE_DEBUG("%s not the sender device %s. You are the %s",
                   this->_device.id,
                   transaction.sender_device_id,
                   transaction.sender_id == this->_self.id ? "sender"
                                                           : "recipient");
        return;
      }

      //auto const& tr = this->sync(transaction.id);
      //if (tr.early_accepted)
      //{
      //  ELLE_DEBUG("the transaction %s has early been accepted", tr.id);

      //  this->update(tr.id,
      //               gap_TransactionStatus::gap_transaction_status_prepared);
      //}
    }

    //void
    //TransactionManager::_accept_transaction(Transaction const& transaction)
    //{
    //  ELLE_TRACE_METHOD(transaction);
    //  this->_ensure_ownership(transaction);
    //  ELLE_ASSERT_EQ(transaction.recipient_id, this->_self.id);

    //  this->_reporter.store("transaction_accept",
    //                        {{MKey::status, "attempt"},
    //                         {MKey::value, transaction.id}});

    //  try
    //  {
    //    this->_meta.update_transaction(transaction.id,
    //                                   plasma::TransactionStatus::accepted,
    //                                   this->_device.id,
    //                                   this->_device.name);
    //    this->_user_manager.swaggers_dirty();
    //  }
    //  CATCH_FAILURE_TO_METRICS("transaction_accept");

    //  this->_reporter.store("transaction_accept",
    //                        {{MKey::status, "succeed"},
    //                         {MKey::value, transaction.id}});
    //}

    //void
    //TransactionManager::_on_transaction_accepted(Transaction const& transaction)
    //{
    //  ELLE_TRACE_METHOD(transaction);
    //  this->_ensure_ownership(transaction);

    //  if (transaction.sender_device_id != this->_device.id)
    //  {
    //    ELLE_DEBUG("%s not the sender device %s. You are the %s",
    //               this->_device.id,
    //               transaction.sender_device_id,
    //               transaction.sender_id == this->_self.id ? "sender"
    //                                                       : "recipient");
    //    return;
    //  }

    //  auto const& tr = this->sync(transaction.id);
    //  if (!tr.early_accepted)
    //  {
    //    ELLE_DEBUG("not early accepted");
    //    // When recipient has rights, allow him to start download.
    //    this->update(tr.id,
    //                 gap_transaction_status_prepared);

    //    // XXX Could be improved.
    //    this->_user_manager.swaggers_dirty();
    //  }
    //}

    //void
    //TransactionManager::_prepare_transaction(Transaction const& transaction)
    //{
    //  ELLE_TRACE_METHOD(transaction);
    //  this->_ensure_ownership(transaction);
    //  ELLE_ASSERT_EQ(transaction.sender_device_id, this->_device.id);

    //  this->_reporter.store("transaction_ready",
    //                        {{MKey::status, "attempt"},
    //                         {MKey::value, transaction.id}});

    //  try
    //  {
    //    this->_network_manager.wait_portal(transaction.network_id);

    //    ELLE_DEBUG("giving '%s' access to the network '%s'",
    //               transaction.recipient_id,
    //               transaction.network_id);

    //    std::string recipient_k =
    //      this->_meta.user(transaction.recipient_id).public_key;

    //    this->_network_manager.add_user(transaction.network_id,
    //                                    this->_self.id,
    //                                    transaction.recipient_id,
    //                                    recipient_k);

    //    ELLE_DEBUG("Giving '%s' permissions on the network to '%s'.",
    //               transaction.recipient_id,
    //               transaction.network_id);

    //    this->_network_manager.set_permissions(
    //      transaction.network_id,
    //      transaction.recipient_id,
    //      recipient_k,
    //      nucleus::neutron::permissions::write);

    //    this->_meta.update_transaction(transaction.id,
    //                                   plasma::TransactionStatus::prepared);
    //  }
    //  CATCH_FAILURE_TO_METRICS("transaction_ready");

    //  this->_reporter.store("transaction_ready",
    //                        {{MKey::status, "succeed"},
    //                         {MKey::value, transaction.id}});
    //}

    //void
    //TransactionManager::_on_transaction_prepared(Transaction const& transaction)
    //{
    //  ELLE_TRACE_METHOD(transaction);
    //  this->_ensure_ownership(transaction);

    //  if (transaction.recipient_device_id != this->_device.id)
    //  {
    //    ELLE_DEBUG("%s not the recipient device %s. You are the %s",
    //               this->_device.id,
    //               transaction.sender_device_id,
    //               transaction.sender_id == this->_self.id ? "sender"
    //                                                       : "recipient");
    //    return;
    //  }

    //  this->_network_manager.add_device(transaction.network_id,
    //                                    this->_device.id);
    //  this->_network_manager.prepare(transaction.network_id);
    //  this->_network_manager.to_directory(
    //    transaction.network_id,
    //    common::infinit::network_shelter(this->_self.id,
    //                                     transaction.network_id));
    //  this->_network_manager.wait_portal(transaction.network_id);

    //  this->update(transaction.id,
    //               gap_transaction_status_started);
    //}

    void
    TransactionManager::_start_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.recipient_device_id, this->_device.id);

      this->_reporter.store("transaction_start",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.id}});

      try
      {
        this->_meta.update_transaction(transaction.id,
                                       plasma::TransactionStatus::started);
      }
      CATCH_FAILURE_TO_METRICS("transaction_start");

      this->_reporter.store("transaction_start",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_started(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction, true);

      this->_network_manager.wait_portal(transaction.network_id);

      struct NotifyOp:
         public Operation
      {
        typedef std::function<void(std::string const&,
                                   std::string const&,
                                   std::string const&,
                                   reactor::Scheduler&)> Notify8infinitFunc;
        typedef std::function<void(std::string const&)> DownloadFilesFunc;
      public:
        TransactionManager& _transaction_manager;
        Transaction const& _transaction;
        Device const& _device;
        Notify8infinitFunc notify_8infinit;
        DownloadFilesFunc download_files;

        NotifyOp(TransactionManager& transaction_manager,
                 Transaction const& transaction,
                 Device const& device,
                 Notify8infinitFunc notify_8infinit,
                 DownloadFilesFunc download_files):
          Operation{"notify_8infinit_" + transaction.id},
          _transaction_manager(transaction_manager),
          _transaction(transaction),
          _device(device),
          notify_8infinit(notify_8infinit),
          download_files(download_files)
        {}

      public:
        void
        _run() override
        {
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
                  this->notify_8infinit(this->_transaction.network_id,
                                        this->_transaction.sender_device_id,
                                        this->_transaction.recipient_device_id,
                                        sched);
                }
                // A parsing bug in gcc (fixed in 4.8.3) make this block
                // mandatory.
                catch (std::exception const&)
                {
                  exception = std::current_exception();
                }
                catch (...)
                {
                  exception = std::current_exception();
                }
              }
            };

            sched.run();
            ELLE_DEBUG("notify finished");
          }
          if (this->cancelled())
            return;
          if (exception != std::exception_ptr{})
          {
            ELLE_ERR("cannot connect infinit instances: %s",
                     elle::exception_string(exception));
            this->_transaction_manager.update(this->_transaction.id,
                                              gap_transaction_status_canceled);
            std::rethrow_exception(exception);
          }
          if (this->_transaction.recipient_device_id == this->_device.id)
          {
            this->download_files(this->_transaction.id);
          }
        }
      };

      NotifyOp::Notify8infinitFunc n =
        std::bind(&NetworkManager::notify_8infinit,
                  &(this->_network_manager),
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3,
                  std::placeholders::_4);
      NotifyOp::DownloadFilesFunc d =
        std::bind(&TransactionManager::_download_files,
                  this,
                  std::placeholders::_1);

      this->_add<NotifyOp>(*this,
                           transaction,
                           this->_device,
                           n,
                           d);
    }

    void
    TransactionManager::_close_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.recipient_device_id, this->_device.id);

      this->_reporter.store("transaction_finish",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.id}});

      try
      {
        this->_meta.update_transaction(transaction.id,
                                       plasma::TransactionStatus::finished);
      }
      CATCH_FAILURE_TO_METRICS("transaction_finish");

      this->_reporter.store("transaction_finish",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_closed(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction, true);

      // Delete networks.
      this->_network_manager.delete_(transaction.network_id,
                                     true /* force */);
    }

    void
    TransactionManager::_cancel_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);

      std::string author{
        transaction.sender_id == this->_self.id ? "sender" : "recipient",};

      this->_reporter.store("transaction_cancel",
                            {{MKey::status, "attempt"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.id}});

      ELLE_SCOPE_EXIT(
        [&]
        {
          this->_cancel_all(transaction.id);
          this->_network_manager.delete_(transaction.network_id,
                                         true);
        }
      );

      try
      {
        this->_meta.update_transaction(transaction.id,
                                       plasma::TransactionStatus::canceled);
      }
      CATCH_FAILURE_TO_METRICS("transaction_cancel");

      this->_reporter.store("transaction_cancel",
                            {{MKey::status, "succeed"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_canceled(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);

      // We can't do check_device = true cause the recipient device is not set
      // until he accepts. So we need to check manualy.
      this->_ensure_ownership(transaction);

      if (transaction.recipient_device_id == this->_device.id ||
          transaction.sender_device_id == this->_device.id)
      {
        this->_cancel_all(transaction.id);
        this->_network_manager.delete_(transaction.network_id, true);
        ELLE_DEBUG("Network %s successfully deleted", transaction.network_id);
      }
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
    TransactionManager::_on_transaction(TransactionNotification const& notif,
                                        bool is_new)
    {
      ELLE_TRACE_FUNCTION(notif.id, is_new);

      // If it's not new, we already has it on our transactions.
      if (!is_new) return;

      auto it = this->all().find(notif.id);

      if (it != this->all().end())
      {
        // The evaluation of transaction is lazy, which means that if your first
        // operation about transactions is create one, at the first evaluation,
        // the new transaction will already be in the transactions map, causing
        // the following warning to appear. Don't care.
        ELLE_WARN("you already have this transaction");
        return;
      }
    }
  }
}
