#include <infinit/metrics/Reporter.hh>

#include <elle/log.hh>

#include <reactor/scheduler.hh>
#include <reactor/Scope.hh>

#include <version.hh>

ELLE_LOG_COMPONENT("infinit.metrics.Reporter");

namespace infinit
{
  namespace metrics
  {

    /*-----------------.
    | Class Attributes |
    `-----------------*/

    std::string Reporter::_client_version = INFINIT_VERSION;
    std::string Reporter::_user_agent = elle::sprintf(
      "Infinit/%s (%s)",
      Reporter::_client_version,
    #ifdef INFINIT_LINUX
      "Linux"
    #elif defined(INFINIT_MACOSX)
      "OS X"
    #elif defined(INFINIT_WINDOWS)
      "Windows"
    #else
    # error "machine not supported"
    #endif
    );

    std::string Reporter::_metric_sender_id = "";

    void
    Reporter::metric_sender_id(std::string const& metric_sender_id)
    {
      Reporter::_metric_sender_id = metric_sender_id;
    }

    Reporter::Reporter(std::string const& name):
      _name(name),
      _metric_available("metric available"),
      _metric_queue()
    {
#ifdef INFINIT_NO_METRICS
      this->_no_metrics = true;
#else
      this->_no_metrics = false;
#endif
      ELLE_DEBUG("%s: creating metrics reporter (%s)", *this, this->name());
    }

    void
    Reporter::name(std::string name)
    {
      this->_name = name;
    }

    Reporter::~Reporter()
    {
      if (this->_poll_thread)
        this->_poll_thread->terminate_now();
    }

    void
    Reporter::start()
    {
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
    Reporter::stop()
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
    }

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    Reporter::transaction_accepted(std::string const& transaction_id)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_accepted,
                                         this,
                                         transaction_id));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_connected(std::string const& transaction_id,
                                    std::string const& connection_method)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_connected,
                                         this,
                                         transaction_id,
                                         connection_method));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_created(std::string const& transaction_id,
                                  std::string const& sender_id,
                                  std::string const& recipient_id,
                                  uint32_t file_count,
                                  uint64_t total_size,
                                  uint32_t message_length)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_created,
                                         this,
                                         transaction_id,
                                         sender_id,
                                         recipient_id,
                                         file_count,
                                         total_size,
                                         message_length));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_ended(std::string const& transaction_id,
                                infinit::oracles::Transaction::Status status,
                                std::string const& info)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_ended,
                                         this,
                                         transaction_id,
                                         status,
                                         info));
      this->_metric_available.open();
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    Reporter::user_favorite(std::string const& user_id)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_favorite,
                                         this,
                                         user_id));
      this->_metric_available.open();
    }

    void
    Reporter::user_login(bool success, std::string const& info)
    {
      if (this->_no_metrics)
        return;
      ELLE_DUMP("%s: got user_login metric", *this);
      this->_metric_queue.push(std::bind(&Reporter::_user_login,
                                         this,
                                         success,
                                         info));
      this->_metric_available.open();
    }

    void
    Reporter::user_logout(bool success, std::string const& info)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_logout,
                                         this,
                                         success,
                                         info));
      this->_metric_available.open();
    }

    void
    Reporter::user_register(bool success, std::string const& info)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_register,
                                         this,
                                         success,
                                         info));
      this->_metric_available.open();
    }

    void
    Reporter::user_unfavorite(std::string const& user_id)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_unfavorite,
                                         this,
                                         user_id));
      this->_metric_available.open();
    }

    /*---------------.
    | Queue Handling |
    `---------------*/
    void
    Reporter::_poll()
    {
      if (this->_no_metrics)
      {
        ELLE_LOG("%s: no metrics, will not poll metrics queue", *this);
        return;
      }
      ELLE_LOG("%s: start polling", *this);
      while (true)
      {
        reactor::wait(this->_metric_available);
        ELLE_ASSERT(!this->_metric_queue.empty());
        ELLE_DEBUG("%s: new metric in %s queue", *this, this->name());
        auto fn = this->_metric_queue.front();
        ELLE_DUMP("%s: run metric for %s", *this, this->name());
        fn();
        this->_metric_queue.pop();
        if (this->_metric_queue.empty())
        {
          ELLE_DUMP("%s: metric queue for %s empty", *this, this->name());
          this->_metric_available.close();
          this->_metric_queue_empty.signal();
        }
      }
    }

    /*--------------------.
    | Attribute Accessors |
    `--------------------*/
    std::string
    Reporter::client_version()
    {
      return Reporter::_client_version;
    }

    std::string
    Reporter::metric_sender_id()
    {
      return Reporter::_metric_sender_id;
    }

    std::string
    Reporter::user_agent()
    {
      return Reporter::_user_agent;
    }

    /*-----------------------------------.
    | Default Transaction Implementation |
    `-----------------------------------*/
    void
    Reporter::_transaction_accepted(std::string const& transaction_id)
    {}

    void
    Reporter::_transaction_connected(std::string const& transaction_id,
                                     std::string const& connection_method)
    {}

    void
    Reporter::_transaction_created(std::string const& transaction_id,
                                   std::string const& sender_id,
                                   std::string const& recipient_id,
                                   uint32_t file_count,
                                   uint64_t total_size,
                                   uint32_t message_length)
    {}

    void
    Reporter::_transaction_ended(std::string const& transaction_id,
                                 infinit::oracles::Transaction::Status status,
                                 std::string const& info)
    {}

    /*----------------------------.
    | Default User Implementation |
    `----------------------------*/
    void
    Reporter::_user_favorite(std::string const& user_id)
    {}

    void
    Reporter::_user_login(bool success,
                          std::string const& info)
    {}

    void
    Reporter::_user_logout(bool success,
                           std::string const& info)
    {}

    void
    Reporter::_user_register(bool success,
                             std::string const& info)
    {}

    void
    Reporter::_user_unfavorite(std::string const& user_id)
    {}

    /*----------.
    | Printable |
    `----------*/
    void
    Reporter::print(std::ostream& stream) const
    {
      stream << elle::sprintf("metrics reporter (%s)", this->name());
    }
  }
}
