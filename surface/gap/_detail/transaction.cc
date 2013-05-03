#include "../State.hh"
#include "TransactionProgress.hh"
#include "Operation.hh"

#include <metrics/Reporter.hh>

#include <common/common.hh>

#include <elle/utility/Time.hh>
#include <elle/memory.hh>
#include <elle/os/path.hh>
#include <elle/os/getenv.hh>
#include <elle/system/Process.hh>
#include <elle/finally.hh>
#include <elle/memory.hh>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>


ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    using MKey = elle::metrics::Key;
    namespace fs = boost::filesystem;

    class State::UploadOperation: public State::Operation
    {
      State& _state;
      plasma::meta::Client& _meta;
      plasma::meta::SelfResponse& _me;
      elle::metrics::Reporter& _reporter;
      std::string _recipient_id_or_email;
      std::unordered_set<std::string> _files;
      std::string _transaction_id;
      std::string _network_id;

    public:
      UploadOperation(State& state,
                      plasma::meta::Client& meta,
                      plasma::meta::SelfResponse& me,
                      elle::metrics::Reporter& reporter,
                      std::string const& recipient_id_or_email,
                      std::unordered_set<std::string> const& files):
        Operation{"upload_files_"},
        _state(state),
        _meta(meta),
        _me(me),
        _reporter(reporter),
        _recipient_id_or_email{recipient_id_or_email},
        _files{files}
      {}

    protected:
      virtual
      void
      _run() override
      {
        ELLE_TRACE_METHOD(this->_network_id, this->_files);


        int size = 0;
        for (auto const& file: this->_files)
          size += this->_state.file_size(file);

        std::string first_file = fs::path(*(this->_files.cbegin())).filename().string();
        elle::utility::Time time; time.Current();
        std::string network_name = elle::sprintf("%s-%s",
                                                 this->_recipient_id_or_email,
                                                 time.nanoseconds);
        this->_network_id = this->_state.create_network(network_name);
        plasma::meta::CreateTransactionResponse res;

        ELLE_DEBUG("(%s): (%s) %s [%s] -> %s throught %s (%s)",
                   this->_state.device_id(),
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
                                                this->_state.device_id());
        }
        catch (...)
        {
          ELLE_DEBUG("transaction creation failed");
          // Something went wrong, we need to destroy the network.
          this->_state.delete_network(this->_network_id, false);
          throw;
        }
        this->_transaction_id = res.created_transaction_id;
        this->_name += this->_transaction_id;
        this->_me.remaining_invitations = res.remaining_invitations;

        this->_reporter.store("transaction_create",
                              {{MKey::status, "attempt"},
                               {MKey::value, this->_transaction_id},
                               {MKey::count, std::to_string(this->_files.size())},
                               {MKey::size, std::to_string(size)}});

        ELLE_DEBUG("created network id is %s", this->_network_id);
        if (this->_state._wait_portal(this->_network_id) == false)
          throw surface::gap::Exception(
            gap_error,
            elle::sprintf("Couldn't find portal to infinit for network %s",
                          this->_network_id));

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
                "-u", this->_state.me()._id,
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
            this->_state.update_transaction(this->_transaction_id,
                                            gap_TransactionStatus::gap_transaction_status_canceled);
            throw;
          }
        }
        CATCH_FAILURE_TO_METRICS("transaction_create");

        this->_reporter.store("transaction_create",
                              {{MKey::status, "succeed"},
                               {MKey::value, this->_transaction_id},
                               {MKey::count, std::to_string(this->_files.size())},
                               {MKey::size, std::to_string(size)}});

        this->_state.update_transaction(this->_transaction_id,
                                        gap_TransactionStatus::gap_transaction_status_created);
      }

      void
      _cancel()
      {
        ELLE_DEBUG("cancelling %s name", this->name());
        this->_state.update_transaction(this->_transaction_id,
                                        gap_TransactionStatus::gap_transaction_status_canceled);

      }
    };

    class State::DownloadOperation: public State::Operation
    {
      State& _state;
      plasma::Transaction _transaction;
      std::string _output_dir;

    public:
      DownloadOperation(State& state,
                        plasma::Transaction const& transaction,
                        std::string const& output_dir)
        : Operation{"download_files_for_" + transaction.transaction_id}
        , _state(state)
        , _transaction(transaction)
        , _output_dir{output_dir}
      {}

    protected:
      virtual
      void
      _run() override
      {
        std::string const& transfer_binary = common::infinit::binary_path("8transfer");

        std::list<std::string> arguments{
          "-n", this->_transaction.network_id,
          "-u", this->_state.me()._id,
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
                     elle::os::path::join(this->_output_dir,
                                          this->_transaction.first_filename));
          }
          else
          {
            ELLE_LOG("Download complete. Your %d files are in '%s'.",
                     this->_transaction.files_count, this->_output_dir);
          }

          this->_state.update_transaction(
            this->_transaction.transaction_id,
            gap_TransactionStatus::gap_transaction_status_finished
          );
        }
        catch (std::exception const& err)
        {
          ELLE_ERR("couldn't receive file %s: %s",
                   this->_transaction.first_filename,
                   err.what());
          this->_state.update_transaction(this->_transaction.transaction_id,
                                          gap_TransactionStatus::gap_transaction_status_canceled);
        }
      }

      void
      _cancel()
      {
        ELLE_DEBUG("cancelling %s name", this->name());
        this->_state.update_transaction(this->_transaction.transaction_id,
                                        gap_TransactionStatus::gap_transaction_status_canceled);

      }
    };

    State::OperationId
    State::send_files(std::string const& recipient_id_or_email,
                      std::unordered_set<std::string> const& files)
    {
      ELLE_TRACE_METHOD(recipient_id_or_email, files);

      if (files.empty())
        throw Exception(gap_no_file, "no files to send");

      return this->_add_operation<UploadOperation>(*this,
                                                   *this->_meta,
                                                   this->_me,
                                                   this->_reporter,
                                                   recipient_id_or_email,
                                                   files);
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

      auto& progress_ptr = this->_transaction_progress[transaction_id];
      if (progress_ptr == nullptr)
        progress_ptr = elle::make_unique<TransactionProgress>();

      ELLE_ASSERT(progress_ptr != nullptr);
      TransactionProgress& progress = *progress_ptr;

      if (progress.process == nullptr)
      {
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
        progress.process = elle::make_unique<elle::system::Process>(
            std::move(pc),
            progress_binary,
            arguments
        );
      }
      ELLE_ASSERT(progress.process != nullptr);

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
          ELLE_WARN("8progress returned an integer greater than 1: %s", progress);
          progress.last_value = 1.0f;
        }
      }

      ELLE_DEBUG("transaction_progress(%s) -> %f",
                 transaction_id,
                 progress.last_value);
      return progress.last_value;
    }

    State::OperationId
    State::_download_files(std::string const& transaction_id)
    {
      auto pair = State::transactions().find(transaction_id);

      ELLE_ASSERT(pair != State::transactions().end());

      if (pair == State::transactions().end())
      {
        throw Exception(gap_error,
                        elle::sprintf("download failure cause %s doesn't exist",
                                      transaction_id));
      }

      Transaction const& trans = pair->second;

      return this->_add_operation<DownloadOperation>(*this,
                                                     trans,
                                                     this->_output_dir);
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
        {gap_transaction_status_canceled,
          {gap_transaction_status_canceled}},
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
        {gap_transaction_status_canceled,
          {gap_transaction_status_canceled}},
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
          status_list->second.find(status) == status_list->second.end())
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
      ELLE_DEBUG("Update transaction '%s': '%s'", transaction_id, status);

      auto const& transaction = this->transaction_sync(transaction_id);

      if (transaction.status == gap_transaction_status_canceled)
      {
        ELLE_WARN("updating %s with canceled status to %s",
                  transaction_id, status);
        return;
      }

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
    State::_ensure_transaction_ownership(Transaction const& transaction,
                                         bool check_devices)
    {
      ELLE_ASSERT(this->_me._id == transaction.sender_id ||
                  this->_me._id == transaction.recipient_id);

      if (check_devices)
      {
        ELLE_ASSERT(this->device_id() == transaction.sender_device_id ||
                    this->device_id() == transaction.recipient_device_id);
      }
    }

    void
    State::_create_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.sender_device_id, this->device_id());

      this->_meta->update_transaction(transaction.transaction_id,
                                      plasma::TransactionStatus::created);
    }

    void
    State::_on_transaction_created(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);
      if (transaction.sender_device_id != this->_device_id)
      {
        ELLE_DEBUG("%s not the sender device %s. You are the %s",
                   this->device_id(),
                   transaction.sender_device_id,
                   transaction.sender_id == this->_me._id ? "sender"
                                                          : "recipient");
        return;
      }

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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.recipient_id, this->_me._id);

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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);

      if (transaction.sender_device_id != this->device_id())
      {
        ELLE_DEBUG("%s not the sender device %s. You are the %s",
                   this->device_id(),
                   transaction.sender_device_id,
                   transaction.sender_id == this->_me._id ? "sender"
                                                          : "recipient");
        return;
      }

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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.sender_device_id, this->device_id());

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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);

      if (transaction.recipient_device_id != this->device_id())
      {
        ELLE_DEBUG("%s not the recipient device %s. You are the %s",
                   this->device_id(),
                   transaction.sender_device_id,
                   transaction.sender_id == this->_me._id ? "sender"
                                                          : "recipient");
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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.recipient_device_id, this->device_id());

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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction, true);

      if (this->_wait_portal(transaction.network_id) == false)
        throw Exception{gap_error, "Couldn't find portal to infinit instance"};

      struct NotifyOp:
        public Operation
      {
      public:
        State& state;
        Transaction transaction;
        std::function<void(Transaction const&, reactor::Scheduler&)>
          notify_8infinit;
        std::function<void(std::string const&)> download_files;

        NotifyOp(State& state,
                 Transaction const& transaction,
                 std::function<void(Transaction const&, reactor::Scheduler&)>
                 notify_8infinit,
                 std::function<void(std::string const&)> download_files):
          Operation{"notify_8infinit_" + transaction.transaction_id},
          state(state),
          transaction(transaction),
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
                  this->notify_8infinit(this->transaction, sched);
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
            try { std::rethrow_exception(exception); }
            catch (std::exception const& err) { msg = err.what(); }
            catch (...) { msg = "Unknown exception type"; }
            ELLE_ERR("cannot connect infinit instances: %s", msg);
            this->state.update_transaction(this->transaction.transaction_id,
                                           gap_transaction_status_canceled);
            std::rethrow_exception(exception);
          }
          if (this->transaction.recipient_device_id == this->state.device_id())
          {
            this->download_files(this->transaction.transaction_id);
          }
        }
      };

      this->_add_operation<NotifyOp>(*this,
                                     transaction,
                                     std::bind(&State::_notify_8infinit,
                                               this,
                                               std::placeholders::_1,
                                               std::placeholders::_2),
                                     std::bind(&State::_download_files,
                                               this,
                                               std::placeholders::_1));
    }

    void
    State::_close_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction);
      ELLE_ASSERT_EQ(transaction.recipient_device_id, this->device_id());

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
      ELLE_TRACE_METHOD(transaction);
      this->_ensure_transaction_ownership(transaction, true);

      // Delete networks.
      this->infinit_instance_manager().stop_network(transaction.network_id);
      this->delete_network(transaction.network_id, true /* force */);
    }

    void
    State::_cancel_transaction(Transaction const& transaction)
    {
      ELLE_TRACE_METHOD(transaction);

      std::string author{
        transaction.sender_id == this->_me._id ? "sender" : "recipient",};

      this->_reporter.store("transaction_cancel",
                            {{MKey::status, "attempt"},
                             {MKey::author, author},
                             {MKey::step, std::to_string(transaction.status)},
                             {MKey::value, transaction.transaction_id}});

      ELLE_SCOPE_EXIT(
        [&] {
          this->_cancel_all_operations(transaction.transaction_id);
          this->delete_network(transaction.network_id, true);
        }
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
      ELLE_TRACE_METHOD(transaction);

      // We can't do check_device = true cause the recipient device is not set
      // until he accepts. So we need to check manualy.
      this->_ensure_transaction_ownership(transaction);

      if (transaction.recipient_device_id == this->device_id() ||
          transaction.sender_device_id == this->device_id())
      {
        this->_cancel_all_operations(transaction.transaction_id);
        this->delete_network(transaction.network_id, true);
        ELLE_DEBUG("Network %s successfully deleted", transaction.network_id);
      }
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
      ELLE_TRACE_METHOD(notif.transaction.transaction_id, is_new);

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
      ELLE_TRACE_METHOD(notif.status);

      auto const& transaction = this->transaction_sync(notif.transaction_id);

      if (transaction.status == gap_transaction_status_canceled &&
          notif.status != gap_transaction_status_canceled)
      {
        ELLE_WARN("we merged a canceled transaction, nothing to do with that.");

        this->update_transaction(notif.transaction_id,
                                 gap_transaction_status_canceled);
        return;
      }

      //this->_transactions->at(notif.transaction_id).status = notif.status;

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
