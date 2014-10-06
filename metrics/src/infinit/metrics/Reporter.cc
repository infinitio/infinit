#include <infinit/metrics/Reporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>

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
    std::string Reporter::_metric_device_id = "";
    std::unordered_map<std::string,std::string> Reporter::_metric_features;

    void
    Reporter::metric_sender_id(std::string const& metric_sender_id)
    {
      Reporter::_metric_sender_id = metric_sender_id;
    }

    void
    Reporter::metric_device_id(std::string const& device_id)
    {
      Reporter::_metric_device_id = device_id;
    }

    void
    Reporter::metric_features(std::unordered_map<std::string,std::string> const& features)
    {
      Reporter::_metric_features = features;
    }

    Reporter::Reporter(std::string const& name):
      _name(name),
      _proxy(),
      _metric_available("metric available"),
      _metric_queue()
    {
      bool in_env = elle::os::inenv("INFINIT_NO_METRICS");
      if (in_env)
        this->_no_metrics = true;
      else
        this->_no_metrics = false;
      ELLE_DEBUG("%s: creating metrics reporter (%s)", *this, this->name());
    }

    void
    Reporter::name(std::string name)
    {
      this->_name = name;
    }

    void
    Reporter::proxy(reactor::network::Proxy const& proxy)
    {
      this->_proxy = proxy;
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
        reactor::wait(this->_metric_queue_empty, 5_sec);
      if (this->_poll_thread)
        this->_poll_thread->terminate_now();
    }

    /*--------------------.
    | Transaction Metrics |
    `--------------------*/
    void
    Reporter::transaction_accepted(std::string const& transaction_id,
                                   bool onboarding)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_accepted,
                                         this,
                                         transaction_id,
                                         onboarding));
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
    Reporter::link_transaction_created(std::string const& transaction_id,
                                       std::string const& sender_id,
                                       int64_t file_count,
                                       int64_t total_size,
                                       uint32_t message_length,
                                       bool onboarding)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_link_transaction_created,
                                         this,
                                         transaction_id,
                                         sender_id,
                                         file_count,
                                         total_size,
                                         message_length,
                                         onboarding));
      this->_metric_available.open();
    }

    void
    Reporter::peer_transaction_created(std::string const& transaction_id,
                                       std::string const& sender_id,
                                       std::string const& recipient_id,
                                       int64_t file_count,
                                       int64_t total_size,
                                       uint32_t message_length,
                                       bool ghost,
                                       bool onboarding)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_peer_transaction_created,
                                         this,
                                         transaction_id,
                                         sender_id,
                                         recipient_id,
                                         file_count,
                                         total_size,
                                         message_length,
                                         ghost,
                                         onboarding));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_ended(std::string const& transaction_id,
                                infinit::oracles::Transaction::Status status,
                                std::string const& info,
                                bool onboarding)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_ended,
                                         this,
                                         transaction_id,
                                         status,
                                         info,
                                         onboarding));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_deleted(std::string const& transaction_id)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_deleted,
                                         this,
                                         transaction_id));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_transfer_begin(std::string const& transaction_id,
                                         TransferMethod method,
                                         float initialization_time)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_transfer_begin,
                                          this,
                                          transaction_id,
                                          method, initialization_time));
      this->_metric_available.open();
    }

    void
    Reporter::transaction_transfer_end(std::string const& transaction_id,
                                       TransferMethod method,
                                       float duration,
                                       uint64_t bytes_transfered,
                                       TransferExitReason reason,
                                       std::string const& message)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_transaction_transfer_end,
                                          this, transaction_id,
                                          method, duration, bytes_transfered,
                                          reason, message));
      this->_metric_available.open();
    }

    void
    Reporter::aws_error(std::string const& transaction_id,
                        std::string const& operation,
                        std::string const& url,
                        unsigned int attempt,
                        int http_status,
                        std::string const& aws_error_code,
                        std::string const& message)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_aws_error,
                                          this, transaction_id, operation,
                                          url, attempt, http_status,
                                          aws_error_code, message));
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

    void
    Reporter::user_heartbeat()
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_heartbeat,
                                         this));
      this->_metric_available.open();
    }

    void
    Reporter::user_first_launch()
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_first_launch, this));
      this->_metric_available.open();
    }

    void
    Reporter::user_proxy(reactor::network::ProxyType proxy_type)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(
        std::bind(&Reporter::_user_proxy, this, proxy_type));
      this->_metric_available.open();
    }

    void
    Reporter::user_crashed()
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(std::bind(&Reporter::_user_crashed, this));
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
        ELLE_TRACE("%s: no metrics, will not poll metrics queue", *this);
        return;
      }
      ELLE_TRACE("%s: start polling", *this);
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
    Reporter::metric_device_id()
    {
      return Reporter::_metric_device_id;
    }

    std::unordered_map<std::string, std::string> const&
    Reporter::metric_features()
    {
      return Reporter::_metric_features;
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
    Reporter::_transaction_accepted(std::string const& transaction_id,
                                    bool onboarding)
    {}

    void
    Reporter::_transaction_connected(std::string const& transaction_id,
                                     std::string const& connection_method)
    {}

    void
    Reporter::_link_transaction_created(std::string const& transaction_id,
                                        std::string const& sender_id,
                                        int64_t file_count,
                                        int64_t total_size,
                                        uint32_t message_length,
                                        bool onboarding)
    {}

    void
    Reporter::_peer_transaction_created(std::string const& transaction_id,
                                        std::string const& sender_id,
                                        std::string const& recipient_id,
                                        int64_t file_count,
                                        int64_t total_size,
                                        uint32_t message_length,
                                        bool ghost,
                                        bool onboarding)
    {}

    void
    Reporter::_transaction_ended(std::string const& transaction_id,
                                 infinit::oracles::Transaction::Status status,
                                 std::string const& info,
                                 bool onboarding)
    {}

    void
    Reporter::_transaction_deleted(std::string const& transaction_id)
    {}

    void
    Reporter::_transaction_transfer_begin(std::string const& transaction_id,
                                          TransferMethod method,
                                          float initialization_time)
    {}

    void
    Reporter::_transaction_transfer_end(std::string const& transaction_id,
                                        TransferMethod method,
                                        float duration,
                                        uint64_t bytes_transfered,
                                        TransferExitReason reason,
                                        std::string const& message)
    {}

    void
    Reporter:: _aws_error(std::string const& transaction_id,
                          std::string const& operation,
                          std::string const& url,
                          unsigned int attempt,
                          int http_status,
                          std::string const& aws_error_code,
                          std::string const& message)
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

    void
    Reporter::_user_heartbeat()
    {}

    void
    Reporter::_user_first_launch()
    {}

    void
    Reporter::_user_proxy(reactor::network::ProxyType proxy_type)
    {}
    void
    Reporter::_user_crashed()
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
