#include <infinit/metrics/CompositeReporter.hh>

#include <elle/log.hh>
#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

ELLE_LOG_COMPONENT("infinit.metrics.CompositeReporter");

namespace infinit
{
  namespace metrics
  {
    CompositeReporter::CompositeReporter():
      Reporter::Reporter("composite reporter")
    {}

    void
    CompositeReporter::start()
    {
      for (auto& reporter: this->_reporters)
        reporter->start();

      this->_metric_available.close();
      auto& sched = *reactor::Scheduler::scheduler();
      this->_poll_thread.reset(
        new reactor::Thread(
          sched,
          "metrics polling thread",
          [&]
          {
            this->_poll();
          }
        )
      );
    }

    void
    CompositeReporter::stop()
    {
      Reporter::stop();
      for (auto& reporter: this->_reporters)
        reporter->stop();
      this->_reporters.clear();
    }

    /*--------------------.
    | Reporter Management |
    `--------------------*/
    void
    CompositeReporter::add_reporter(
      std::unique_ptr<Reporter>&& reporter)
    {
      this->_reporters.emplace_back(std::move(reporter));
    }

    /*----------------.
    | Dispatch Metric |
    `----------------*/

    void
    CompositeReporter::_dispatch(std::function<void(Reporter*)> fn)
    {
      ELLE_TRACE_SCOPE("%s: dispatching metrics to all reporters", *this);
      elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
      {
        for (auto& reporter: this->_reporters)
        {
          scope.run_background(
            elle::sprintf("%s: %s", *this, *reporter),
            [fn, this, &reporter]
            {
              ELLE_DUMP("%s: sending metric using %s", *this, *reporter);
              fn(reporter.get());
            });
        }
        reactor::wait(scope);
      };
    }

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    CompositeReporter::_transaction_accepted(std::string const& transaction_id,
                                             bool onboarding)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_accepted,
                                std::placeholders::_1,
                                transaction_id,
                                onboarding));
    }

    void
    CompositeReporter::_transaction_connected(std::string const& transaction_id,
                           std::string const& connection_method)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_connected,
                                std::placeholders::_1,
                                transaction_id,
                                connection_method));
    }

    void
    CompositeReporter::_link_transaction_created(
      std::string const& transaction_id,
      std::string const& sender_id,
      int64_t file_count,
      int64_t total_size,
      uint32_t message_length,
      bool onboarding)
    {
      this->_dispatch(std::bind(&Reporter::_link_transaction_created,
                                std::placeholders::_1,
                                transaction_id,
                                sender_id,
                                file_count,
                                total_size,
                                message_length,
                                onboarding));
    }

    void
    CompositeReporter::_peer_transaction_created(
      std::string const& transaction_id,
      std::string const& sender_id,
      std::string const& recipient_id,
      int64_t file_count,
      int64_t total_size,
      uint32_t message_length,
      bool ghost,
      bool onboarding)
    {
      this->_dispatch(std::bind(&Reporter::_peer_transaction_created,
                                std::placeholders::_1,
                                transaction_id,
                                sender_id,
                                recipient_id,
                                file_count,
                                total_size,
                                message_length,
                                ghost,
                                onboarding));
    }

    void
    CompositeReporter::_transaction_ended(
      std::string const& transaction_id,
      infinit::oracles::Transaction::Status status,
      std::string const& info,
      bool onboarding
    )
    {
      this->_dispatch(std::bind(&Reporter::_transaction_ended,
                      std::placeholders::_1,
                      transaction_id,
                      status,
                      info,
                      onboarding));
    }


     void
    CompositeReporter::_transaction_deleted(std::string const& transaction_id)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_deleted,
                      std::placeholders::_1,
                      transaction_id));
    }

    void
    CompositeReporter::_transaction_transfer_begin(std::string const& transaction_id,
                                                   TransferMethod method,
                                                   float initialization_time)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_transfer_begin,
                                std::placeholders::_1,
                                transaction_id,
                                method,
                                initialization_time));
    }

    void
    CompositeReporter::_transaction_transfer_end(std::string const& transaction_id,
                                                 TransferMethod method,
                                                 float duration,
                                                 uint64_t bytes_transfered,
                                                 TransferExitReason reason,
                                                 std::string const& message)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_transfer_end,
                                std::placeholders::_1,
                                transaction_id, method, duration,
                                bytes_transfered, reason, message));
    }

    void
    CompositeReporter:: _aws_error(std::string const& transaction_id,
                                   std::string const& operation,
                                   std::string const& url,
                                   unsigned int attempt,
                                   int http_status,
                                   std::string const& aws_error_code,
                                   std::string const& message)
    {
       this->_dispatch(std::bind(&Reporter::_aws_error,
                                 std::placeholders::_1,
                                 transaction_id, operation, url, attempt,
                                 http_status, aws_error_code, message));
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    CompositeReporter::_user_favorite(std::string const& user_id)
    {
      this->_dispatch(std::bind(&Reporter::_user_favorite,
                                std::placeholders::_1,
                                user_id));
    }

    void
    CompositeReporter::_user_login(bool success, std::string const& info)
    {
      this->_dispatch(std::bind(&Reporter::_user_login,
                                std::placeholders::_1,
                                success,
                                info));
    }

    void
    CompositeReporter::_user_logout(bool success, std::string const& info)
    {
      this->_dispatch(std::bind(&Reporter::_user_logout,
                                std::placeholders::_1,
                                success,
                                info));
    }

    void
    CompositeReporter::_user_register(bool success, std::string const& info)
    {
      this->_dispatch(std::bind(&Reporter::_user_register,
                                std::placeholders::_1,
                                success,
                                info));
    }

    void
    CompositeReporter::_user_unfavorite(std::string const& user_id)
    {
      this->_dispatch(std::bind(&Reporter::_user_unfavorite,
                                std::placeholders::_1,
                                user_id));
    }

    void
    CompositeReporter::_user_heartbeat()
    {
      this->_dispatch(std::bind(&Reporter::_user_heartbeat,
                                std::placeholders::_1));
    }
  }
}
