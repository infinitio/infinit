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
    #elif defined(INFINIT_IOS)
      "iOS"
    #elif defined(INFINIT_WINDOWS)
      "Windows"
    #elif defined(INFINIT_ANDROID)
      "Android"
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
      this->_push(std::bind(&Reporter::_transaction_accepted,
                                         this,
                                         transaction_id,
                                         onboarding));
    }

    void
    Reporter::transaction_connected(std::string const& transaction_id,
                                    std::string const& connection_method,
                                    int attempt)
    {
      this->_push(std::bind(&Reporter::_transaction_connected,
                                         this,
                                         transaction_id,
                                         connection_method,
                                         attempt));
    }

    void
    Reporter::link_transaction_created(std::string const& transaction_id,
                                       std::string const& sender_id,
                                       int64_t file_count,
                                       int64_t total_size,
                                       uint32_t message_length,
                                       bool screenshot,
                                       std::vector<std::string> extensions,
                                       bool onboarding)
    {
      this->_push(std::bind(&Reporter::_link_transaction_created,
                                         this,
                                         transaction_id,
                                         sender_id,
                                         file_count,
                                         total_size,
                                         message_length,
                                         screenshot,
                                         extensions,
                                         onboarding));
    }

    void
    Reporter::peer_transaction_created(std::string const& transaction_id,
                                       std::string const& sender_id,
                                       std::string const& recipient_id,
                                       int64_t file_count,
                                       int64_t total_size,
                                       uint32_t message_length,
                                       bool ghost,
                                       bool onboarding,
                                       std::vector<std::string> extensions
                                       )
    {
      this->_push(std::bind(&Reporter::_peer_transaction_created,
                                         this,
                                         transaction_id,
                                         sender_id,
                                         recipient_id,
                                         file_count,
                                         total_size,
                                         message_length,
                                         ghost,
                                         onboarding,
                                         extensions));
    }

    void
    Reporter::transaction_ended(std::string const& transaction_id,
                                infinit::oracles::Transaction::Status status,
                                std::string const& info,
                                bool onboarding,
                                bool caused_by_user)
    {
      this->_push(std::bind(&Reporter::_transaction_ended,
                                         this,
                                         transaction_id,
                                         status,
                                         info,
                                         onboarding,
                                         caused_by_user));
    }

    void
    Reporter::transaction_deleted(std::string const& transaction_id)
    {
      this->_push(std::bind(&Reporter::_transaction_deleted,
                                         this,
                                         transaction_id));
    }

    void
    Reporter::transaction_transfer_begin(std::string const& transaction_id,
                                         TransferMethod method,
                                         float initialization_time,
                                         int attempt)
    {
      this->_push(std::bind(&Reporter::_transaction_transfer_begin,
                                          this,
                                          transaction_id,
                                          method, initialization_time,
                                          attempt));
    }

    void
    Reporter::transaction_transfer_end(std::string const& transaction_id,
                                       TransferMethod method,
                                       float duration,
                                       uint64_t bytes_transfered,
                                       TransferExitReason reason,
                                       std::string const& message,
                                       int attempt)
    {
      this->_push(std::bind(&Reporter::_transaction_transfer_end,
                                          this, transaction_id,
                                          method, duration, bytes_transfered,
                                          reason, message, attempt));
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
      this->_push(std::bind(&Reporter::_aws_error,
                                          this, transaction_id, operation,
                                          url, attempt, http_status,
                                          aws_error_code, message));
    }

    /*-------------.
    | User Metrics |
    `-------------*/
    void
    Reporter::user_favorite(std::string const& user_id)
    {
      this->_push(std::bind(&Reporter::_user_favorite,
                                         this,
                                         user_id));
    }

    void
    Reporter::user_login(bool success, std::string const& info)
    {
      ELLE_DUMP("%s: got user_login metric", *this);
      this->_push(std::bind(&Reporter::_user_login,
                                         this,
                                         success,
                                         info));
    }

    void
    Reporter::facebook_connect(bool success, std::string const& info)
    {
      ELLE_DUMP("%s: got facebook_connect metric", *this);
      this->_push(std::bind(&Reporter::_facebook_connect,
                                         this,
                                         success,
                                         info));
    }

    void
    Reporter::user_logout(bool success, std::string const& info)
    {
      this->_push(std::bind(&Reporter::_user_logout,
                                         this,
                                         success,
                                         info));
    }

    void
    Reporter::user_register(bool success,
                            std::string const& info,
                            std::string const& with,
                            boost::optional<std::string> ghost_code,
                            boost::optional<std::string> referral_code)
    {
      this->_push(std::bind(&Reporter::_user_register,
                            this,
                            success,
                            info,
                            with,
                            ghost_code,
                            referral_code));
    }

    void
    Reporter::user_unfavorite(std::string const& user_id)
    {
      this->_push(std::bind(&Reporter::_user_unfavorite,
                                         this,
                                         user_id));
    }

    void
    Reporter::user_heartbeat()
    {
      this->_push(std::bind(&Reporter::_user_heartbeat, this));
    }

    void
    Reporter::user_first_launch()
    {
      this->_push(std::bind(&Reporter::_user_first_launch, this));
    }

    void
    Reporter::user_proxy(reactor::network::ProxyType proxy_type)
    {
      this->_push(
        std::bind(&Reporter::_user_proxy, this, proxy_type));
    }

    void
    Reporter::user_crashed()
    {
      this->_push(std::bind(&Reporter::_user_crashed, this));
    }

    void
    Reporter::user_changed_download_dir(bool fallback)
    {
      this->_push(
        std::bind(&Reporter::_user_changed_download_dir, this, fallback));
    }

    void
    Reporter::user_used_ghost_code(bool success,
                                   std::string const& code,
                                   bool link,
                                   std::string const& fail_reason)
    {
      this->_push(std::bind(
        &Reporter::_user_used_ghost_code, this, success, code, link,
        fail_reason));
    }

    void
    Reporter::user_sent_invitation_message(bool success,
                                           std::string const& code,
                                           std::string const& method,
                                           std::string const& fail_reason)
    {
      this->_push(std::bind(
        &Reporter::_user_sent_invitation_message, this, success, code, method,
        fail_reason));
    }

    void
    Reporter::link_quota_exceeded(uint64_t size,
                                  uint64_t current,
                                  uint64_t total)
    {
      this->_push(std::bind(&Reporter::_link_quota_exceeded,
                            this, size, current, total));
    }

    void
    Reporter::send_to_self_limit_reached(uint64_t limit)
    {
      this->_push(std::bind(
        &Reporter::_send_to_self_limit_reached, this, limit));
    }

    void
    Reporter::file_transfer_limit_reached(uint64_t limit,
                                          uint64_t transfer_size)
    {
      this->_push(std::bind(
        &Reporter::_file_transfer_limit_reached, this, limit, transfer_size));
    }

    void
    Reporter::ui(std::string const& event,
                 std::string const& from,
                 Additional const& additional)
    {
      this->_push(std::bind(&Reporter::_ui, this, event, from, additional));
    }

    /*---------------.
    | Queue Handling |
    `---------------*/
    void
    Reporter::_push(Metric const& metric)
    {
      if (this->_no_metrics)
        return;
      this->_metric_queue.push(metric);
      this->_metric_available.open();
    }

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
                                     std::string const& connection_method,
                                     int attempt)
    {}

    void
    Reporter::_link_transaction_created(std::string const& transaction_id,
                                        std::string const& sender_id,
                                        int64_t file_count,
                                        int64_t total_size,
                                        uint32_t message_length,
                                        bool screenshot,
                                        std::vector<std::string> extensions,
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
                                        bool onboarding,
                                        std::vector<std::string> extensions
                                        )
    {}

    void
    Reporter::_transaction_ended(std::string const& transaction_id,
                                 infinit::oracles::Transaction::Status status,
                                 std::string const& info,
                                 bool onboarding,
                                 bool caused_by_user)
    {}

    void
    Reporter::_transaction_deleted(std::string const& transaction_id)
    {}

    void
    Reporter::_transaction_transfer_begin(std::string const& transaction_id,
                                          TransferMethod method,
                                          float initialization_time,
                                          int attempt)
    {}

    void
    Reporter::_transaction_transfer_end(std::string const& transaction_id,
                                        TransferMethod method,
                                        float duration,
                                        uint64_t bytes_transfered,
                                        TransferExitReason reason,
                                        std::string const& message,
                                        int attempt)
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
    Reporter::_facebook_connect(bool success,
                                std::string const& info)
    {}

    void
    Reporter::_user_logout(bool success,
                           std::string const& info)
    {}

    void
    Reporter::_user_register(bool success,
                             std::string const& info,
                             std::string const& with,
                             boost::optional<std::string> ghost_code,
                             boost::optional<std::string> referral_code)
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

    void
    Reporter::_user_changed_download_dir(bool fallback)
    {}

    void
    Reporter::_user_used_ghost_code(bool success,
                                    std::string const& code,
                                    bool link,
                                    std::string const& fail_reason)
    {}

    void
    Reporter::_user_sent_invitation_message(bool success,
                                            std::string const& code,
                                            std::string const& method,
                                            std::string const& fail_reason)
    {}

    void
    Reporter::_link_quota_exceeded(uint64_t size,
                                   uint64_t current,
                                   uint64_t quota)
    {}

    void
    Reporter::_send_to_self_limit_reached(uint64_t limit)
    {}

    void
    Reporter::_file_transfer_limit_reached(uint64_t limit,
                                           uint64_t transfer_size)
    {}

    /*--------------------------.
    | Default UI Implementation |
    `--------------------------*/

    void
    Reporter::_ui(std::string const& event,
                  std::string const& from,
                  Additional const&)
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
