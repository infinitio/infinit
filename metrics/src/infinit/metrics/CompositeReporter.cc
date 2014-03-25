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
      if (!this->_metric_queue.empty())
      {
        elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
        {
          scope.run_background("wait for metrics queue", [&]
          {
            reactor::wait(this->_metric_queue_empty);
            scope.terminate_now();
          });
          scope.wait(5_sec);
        };
      }
      if (this->_poll_thread)
        this->_poll_thread->terminate_now();

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
    CompositeReporter::_transaction_accepted(std::string const& transaction_id)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_accepted,
                                std::placeholders::_1,
                                transaction_id));
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
    CompositeReporter::_transaction_created(std::string const& transaction_id,
                                            std::string const& sender_id,
                                            std::string const& recipient_id,
                                            int64_t file_count,
                                            int64_t total_size,
                                            uint32_t message_length)
    {
      this->_dispatch(std::bind(&Reporter::_transaction_created,
                                std::placeholders::_1,
                                transaction_id,
                                sender_id,
                                recipient_id,
                                file_count,
                                total_size,
                                message_length));
    }

    void
    CompositeReporter::_transaction_ended(
      std::string const& transaction_id,
      infinit::oracles::Transaction::Status status,
      std::string const& info
    )
    {
        this->_dispatch(std::bind(&Reporter::_transaction_ended,
                        std::placeholders::_1,
                        transaction_id,
                        status,
                        info));
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
  }
}
