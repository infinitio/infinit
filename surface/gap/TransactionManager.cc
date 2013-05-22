#include "TransactionManager.hh"
#include <surface/gap/metrics.hh>

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

    size_t
    total_size(std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_FUNCTION(files);

      size_t size = 0;
      ELLE_DEBUG("check files");
      {
        for (auto const& file: files)
        {
          ELLE_DEBUG("castor");
          auto _size = elle::os::file::size(file);
          ELLE_DEBUG("%s: %i", file, _size);
          size += _size;
        }
      }
      return size;
    }

    class UploadOperation: public OperationManager::Operation
    {
      TransactionManager& _transaction_manager;
      NetworkManager& _network_manager;
      elle::metrics::Reporter& _reporter;
      plasma::meta::Client& _meta;
      plasma::meta::SelfResponse& _me;
      std::string _device_id;
      std::string _recipient_id_or_email;
      std::unordered_set<std::string> _files;
      std::string _transaction_id;
      std::string _network_id;

    public:
      UploadOperation(TransactionManager& transaction_manager,
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

    protected:
      virtual
      void
      _run() override
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
      _cancel()
      {
        ELLE_DEBUG("cancelling %s name", this->name());
        this->_transaction_manager.update(
          this->_transaction_id,
          gap_TransactionStatus::gap_transaction_status_canceled);

      }
    };

    class DownloadOperation: public OperationManager::Operation
    {
      TransactionManager& _transaction_manager;
      plasma::meta::SelfResponse const& _me;
      plasma::Transaction const& _transaction;

    public:
      DownloadOperation(TransactionManager& transaction_manager,
                        plasma::meta::SelfResponse const& me,
                        plasma::Transaction const& transaction)
        : Operation{"download_files_for_" + transaction.id}
        , _transaction_manager(transaction_manager)
        , _me(me)
        , _transaction(transaction)
      {}

    protected:
      virtual
      void
      _run() override
      {
        std::string const& transfer_binary =
          common::infinit::binary_path("8transfer");

        std::list<std::string> arguments{
          "-n", this->_transaction.network_id,
          "-u", this->_me.id,
          "--path", this->_transaction_manager.output_dir(),
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
            ELLE_DEBUG("Waiting transfer --from process to finish");
            while (p.running())
            {
              if (p.wait_status(elle::system::Process::Milliseconds(10)) != 0)
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

          if (this->_transaction.files_count == 1)
          {
            ELLE_LOG("Download complete. Your file is at '%s'.",
                     elle::os::path::join(
                       this->_transaction_manager.output_dir(),
                       this->_transaction.first_filename));
          }
          else
          {
            ELLE_LOG("Download complete. Your %d files are in '%s'.",
                     this->_transaction.files_count,
                     this->_transaction_manager.output_dir());
          }

          this->_transaction_manager.update(
            this->_transaction.id,
            gap_TransactionStatus::gap_transaction_status_finished
            );
        }
        catch (std::exception const& err)
        {
          ELLE_ERR("couldn't receive file %s: %s",
                   this->_transaction.first_filename,
                   err.what());
          this->_transaction_manager.update(
            this->_transaction.id,
            gap_TransactionStatus::gap_transaction_status_canceled);
        }
      }

      void
      _cancel()
      {
        ELLE_DEBUG("cancelling %s name", this->name());
        this->_transaction_manager.update(
          this->_transaction.id,
          gap_TransactionStatus::gap_transaction_status_canceled);

      }
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
      this->_notification_manager.transaction_status_callback(
        [&] (TransactionStatusNotification const &n, bool) -> void
        {
          this->_on_transaction_status(n);
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
                        "directory doesn't exisxt."};

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
          {gap_transaction_status_prepared,
           gap_transaction_status_created,
           gap_transaction_status_canceled,}},
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

      auto list = (user_id == transaction.recipient_id)
        ? _recipient_status_update
        : _sender_status_update;

      auto const& status_list = list.find(
        (gap_TransactionStatus) transaction.status);

      if (status_list == list.end() ||
          status_list->second.find(status) == status_list->second.end())
      {
        ELLE_WARN("You are not allowed to change status from %s to %s",
                  transaction.status, status);
        return false;
      }

      return true;
    }

    void
    TransactionManager::update(std::string const& id,
                               gap_TransactionStatus status)
    {
      ELLE_TRACE_METHOD(id, status);

      auto const& transaction = this->sync(id);

      ELLE_DEBUG("transaction: %s", transaction);

      if (transaction.status == gap_transaction_status_canceled)
      {
        ELLE_WARN("updating %s with canceled status to %s",
                  id, status);
        return;
      }

      if (!_check_action_is_available(this->_self.id, transaction, status))
        throw Exception(gap_api_error,
                        elle::sprintf("you aren't allowed to change "
                                      "transaction status from %s to %s",
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
    } // !update()

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

      auto const& tr = this->sync(transaction.id);
      if (tr.early_accepted)
      {
        ELLE_DEBUG("the transaction %s has early been accepted", tr.id);

        this->update(tr.id,
                     gap_TransactionStatus::gap_transaction_status_prepared);
      }
    }

    void
    TransactionManager::_accept_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.recipient_id, this->_self.id);

      this->_reporter.store("transaction_accept",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.id}});

      try
      {
        this->_meta.update_transaction(transaction.id,
                                       plasma::TransactionStatus::accepted,
                                       this->_device.id,
                                       this->_device.name);
        this->_user_manager.swaggers_dirty();
      }
      CATCH_FAILURE_TO_METRICS("transaction_accept");

      this->_reporter.store("transaction_accept",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});

    }

    void
    TransactionManager::_on_transaction_accepted(Transaction const& transaction)
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

      auto const& tr = this->sync(transaction.id);
      if (!tr.early_accepted)
      {
        ELLE_DEBUG("not early accepted");
        // When recipient has rights, allow him to start download.
        this->update(tr.id,
                     gap_transaction_status_prepared);

        // XXX Could be improved.
        this->_user_manager.swaggers_dirty();
      }
    }

    void
    TransactionManager::_prepare_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.sender_device_id, this->_device.id);

      this->_reporter.store("transaction_ready",
                            {{MKey::status, "attempt"},
                             {MKey::value, transaction.id}});

      try
      {
        this->_network_manager.wait_portal(transaction.network_id);

        ELLE_DEBUG("giving '%s' access to the network '%s'",
                   transaction.recipient_id,
                   transaction.network_id);

        std::string recipient_k =
          this->_meta.user(transaction.recipient_id).public_key;

        this->_network_manager.add_user(transaction.network_id,
                                        this->_self.id,
                                        transaction.recipient_id,
                                        recipient_k);

        ELLE_DEBUG("Giving '%s' permissions on the network to '%s'.",
                   transaction.recipient_id,
                   transaction.network_id);

        this->_network_manager.set_permissions(
          transaction.network_id,
          transaction.recipient_id,
          recipient_k,
          nucleus::neutron::permissions::write);

        this->_meta.update_transaction(transaction.id,
                                       plasma::TransactionStatus::prepared);
      }
      CATCH_FAILURE_TO_METRICS("transaction_ready");

      this->_reporter.store("transaction_ready",
                            {{MKey::status, "succeed"},
                             {MKey::value, transaction.id}});
    }

    void
    TransactionManager::_on_transaction_prepared(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_ownership(transaction);

      if (transaction.recipient_device_id != this->_device.id)
      {
        ELLE_DEBUG("%s not the recipient device %s. You are the %s",
                   this->_device.id,
                   transaction.sender_device_id,
                   transaction.sender_id == this->_self.id ? "sender"
                                                           : "recipient");
        return;
      }

      this->_network_manager.add_device(transaction.network_id,
                                        this->_device.id);
      this->_network_manager.prepare(transaction.network_id);
      this->_network_manager.to_directory(
        transaction.network_id,
        common::infinit::network_shelter(this->_self.id,
                                         transaction.network_id));
      this->_network_manager.wait_portal(transaction.network_id);

      this->update(transaction.id,
                   gap_transaction_status_started);
    }

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
                catch (std::exception const& e)
                {
                  exception = std::make_exception_ptr(e);
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
            std::string msg;
            try
            {
              std::rethrow_exception(exception);
            }
            catch (std::exception const& err)
            {
              msg = err.what();
            }
            catch (...)
            {
              msg = "Unknown exception type";
            }
            ELLE_ERR("cannot connect infinit instances: %s", msg);
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
      ELLE_TRACE_FUNCTION(notif.transaction.id, is_new);

      // If it's not new, we already has it on our transactions.
      if (!is_new) return;

      auto it = this->all().find(notif.transaction.id);

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

    void
    TransactionManager::_on_transaction_status(
      TransactionStatusNotification const& notif)
    {
      ELLE_TRACE_FUNCTION(notif.status);

      auto const pair = all().find(notif.transaction_id);

      if (pair == all().end())
      {
        // Something went wrong.
        auto transaction = this->sync(notif.transaction_id);

        if (transaction.status == gap_transaction_status_canceled ||
            transaction.status == gap_transaction_status_finished)
        {
          ELLE_WARN("we merged a canceled transaction, nothing to do.");
          return;
        }
        this->_transactions([&notif, &transaction] (TransactionMapPtr& map) {
            (*map)[notif.transaction_id] = transaction;
        });
      }

      if (pair->second.status == gap_transaction_status_canceled ||
          pair->second.status == gap_transaction_status_finished)
      {
        ELLE_WARN("recieved a status update for a canceled or finished " \
                  "transaction");
        return;
      }

      this->_transactions([&notif] (TransactionMapPtr& map) {
        map->at(notif.transaction_id).status = notif.status;
      });

      auto const& transaction = this->one(notif.transaction_id);

      switch((plasma::TransactionStatus) notif.status)
      {
        case plasma::TransactionStatus::accepted:
          // We update the transaction from meta.
          // XXX: we should have it from transaction_notification.
          this->sync(notif.transaction_id);
          this->_on_transaction_accepted(transaction);
          break;
        case plasma::TransactionStatus::created:
          // We update the transaction from meta.
          // XXX: we should have it from transaction_notification.
          this->sync(notif.transaction_id);
          this->_on_transaction_created(transaction);
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
