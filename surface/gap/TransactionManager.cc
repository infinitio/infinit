#include "TransactionManager.hh"

#include "CreateTransactionOperation.hh"
#include "PrepareTransactionOperation.hh"
#include "DownloadOperation.hh"
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

      return this->_add<CreateTransactionOperation>(
          *this,
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

      if (tr.status == plasma::TransactionStatus::finished)
        return 1.0f;
      else if (tr.status == plasma::TransactionStatus::canceled)
        return 0.0f;
      else if (tr.status != plasma::TransactionStatus::started)
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


    void
    TransactionManager::update(std::string const& transaction_id,
                               plasma::TransactionStatus status)
    {
      ELLE_TRACE("set status %s on transaction %s", status, transaction_id);
      this->_meta.update_transaction(transaction_id, status);
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

      if (notif.sender_id == this->_self.id)
      {
        if (notif.sender_device_id != this->_device.id)
        {
          // ELLE_ASSERT(
          //     false,
          //     "got a transaction notif that does not involve my device: %s",
          //     notif);
          ELLE_WARN("XXX Should be an assert: got device unrelated notif");
          return;
        }
        if (notif.status == plasma::TransactionStatus::created)
          this->_prepare_upload(notif);
        else if (notif.status == plasma::TransactionStatus::started &&
                 notif.accepted &&
                 this->_user_manager.device_status(notif.recipient_device_id))
          this->_start_upload(notif);
      }
      else if (notif.recipient_id == this->_self.id)
      {
        if (notif.recipient_device_id != this->_device.id)
        {
          // ELLE_ASSERT(
          //     false,
          //     "got a transaction notif that does not involve my device: %s",
          //     notif);
          ELLE_WARN("XXX Should be an assert: got device unrelated notif");
          return;
        }
        if (notif.status == plasma::TransactionStatus::started &&
            notif.accepted &&
            this->_user_manager.device_status(notif.sender_device_id))
          this->_start_download(notif);
      }
      else
      {
        ELLE_WARN("got a transaction notif not related to me: %s", notif);
        return;
      }
      // Ensure map is not null
      this->all();

      this->_transactions([&notif] (TransactionMapPtr& ptr) {
          (*ptr)[notif.id] = notif;
      });
    }


      void
      TransactionManager::_prepare_upload(Transaction const& transaction)
      {
        auto& s = this->_states([&] (StateMap& map)-> State& {
            return map[transaction.id];
        });

        if (s.state == State::none)
        {
          ELLE_DEBUG("prepare transaction %s", transaction)
          s.operation = this->_add<PrepareTransactionOperation>(
            *this,
            this->_network_manager,
            this->_meta,
            this->_reporter,
            this->_self,
            transaction);
          s.state = State::preparing;
        }
        else
        {
          ELLE_DEBUG("do not prepare %s, already in state %d",
                     transaction,
                     s.state);
        }
      }

      void
      TransactionManager::_start_upload(Transaction const& transaction)
      {
        auto& s = this->_states([&] (StateMap& map)-> State& {
            return map[transaction.id];
        });
        if (s.state == State::preparing &&
            this->status(s.operation) == OperationStatus::success)
        {
          s.operation = this->_add<UploadOperation>(
            transaction.id,
            std::bind(&NetworkManager::notify_8infinit,
                      &(this->_network_manager),
                      transaction.network_id,
                      transaction.sender_device_id,
                      transaction.recipient_device_id));
          s.state = State::running;
          s.tries += 1;
        }
      }

      void
      TransactionManager::_start_download(Transaction const& transaction)
      {
        auto& s = this->_states([&] (StateMap& map)-> State& {
            return map[transaction.id];
        });
        if (s.state == State::none)
        {
          s.operation = this->_add<DownloadOperation>(
              *this,
              this->_network_manager,
              this->_self,
              transaction,
              std::bind(&NetworkManager::notify_8infinit,
                        &(this->_network_manager),
                        transaction.network_id,
                        transaction.sender_device_id,
                        transaction.recipient_device_id));
          s.state = State::running;
          s.tries += 1;
        }
      }
  }
}
