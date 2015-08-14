#ifndef INFINIT_METRICS_JSON_REPORTER_HH
# define INFINIT_METRICS_JSON_REPORTER_HH

# include <elle/json/json.hh>

# include <reactor/http/Request.hh>

# include <infinit/metrics/Reporter.hh>

namespace infinit
{
  namespace metrics
  {
    enum class JSONKey
    {
      attempt_number,
      aws_error_code,
      by_user,
      bytes_transfered,
      connection_method,
      device_id,
      duration,
      event,
      exit_reason,
      fail_reason,
      fallback,
      features,
      file_count,
      ghost,
      ghost_code,
      ghost_code_link,
      http_status,
      how_ended,
      initialization_time,
      limit,
      message,
      message_length,
      method,
      metric_sender_id,
      onboarding,
      operation,
      plan,
      proxy_type,
      recipient_id,
      referral_code,
      screenshot,
      sender_id,
      status,
      timestamp,
      total_size,
      transaction_id,
      transaction_type,
      transfer_method,
      url,
      user_agent,
      version,
      who,
      quota,
      used_storage,
      extensions,
    };

    class JSONReporter:
      public Reporter
    {
    public:
      JSONReporter(std::string const& name,
                   reactor::http::StatusCode expected_status =
                   reactor::http::StatusCode::OK);

      virtual
      ~JSONReporter() = default;

    /*-----.
    | Send |
    `-----*/
    protected:
      virtual
      void
      _post(std::string const& destination,
            elle::json::Object data);
      virtual
      std::string
      _url(std::string const& destination) const = 0;
    private:
      void
      _send(std::string const& destination,
            elle::json::Object data);

    /// Implementation of transaction metrics.
    private:
      void
      _transaction_accepted(std::string const& transaction_id,
                            bool onboarding) override;

      void
      _transaction_connected(std::string const& transaction_id,
                             std::string const& connection_method,
                             int attempt) override;

      void
      _link_transaction_created(std::string const& transaction_id,
                                std::string const& sender_id,
                                int64_t file_count,
                                int64_t total_size,
                                uint32_t message_length,
                                bool screenshot,
                                std::vector<std::string> extensions,
                                bool onboarding) override;

      void
      _peer_transaction_created(std::string const& transaction_id,
                                std::string const& sender_id,
                                std::string const& recipient_id,
                                int64_t file_count,
                                int64_t total_size,
                                uint32_t message_length,
                                bool ghost,
                                bool onboarding,
                                std::vector<std::string> extensions
                                ) override;

      void
      _transaction_ended(std::string const& transaction_id,
                         infinit::oracles::Transaction::Status status,
                         std::string const& info,
                         bool onboarding,
                         bool caused_by_user) override;

      void
      _transaction_deleted(std::string const& transaction_id) override;


      void
      _transaction_transfer_begin(std::string const& transaction_id,
                                 TransferMethod method,
                                 float initialization_time,
                                 int attempt) override;

      void
      _transaction_transfer_end(std::string const& transaction_id,
                               TransferMethod method,
                               float duration,
                               uint64_t bytes_transfered,
                               TransferExitReason reason,
                               std::string const& message,
                               int attempt) override;
      void
      _aws_error(std::string const& transaction_id,
                std::string const& operation,
                std::string const& url,
                unsigned int attempt,
                int http_status,
                std::string const& aws_error_code,
                std::string const& message) override;

    /// Implementation of user metrics.
    private:
      void
      _user_favorite(std::string const& user_id) override;

      void
      _user_login(bool success, std::string const& info) override;

      void
      _facebook_connect(bool success, std::string const& info) override;

      void
      _user_logout(bool success, std::string const& info) override;

      void
      _user_register(bool success,
                     std::string const& info,
                     std::string const& with,
                     boost::optional<std::string> ghost_code,
                     boost::optional<std::string> referral_code) override;

      void
      _user_unfavorite(std::string const& user_id) override;

      void
      _user_heartbeat() override;

      void
      _user_first_launch() override;

      void
      _user_proxy(reactor::network::ProxyType proxy_type) override;

      void
      _user_crashed() override;

      void
      _user_changed_download_dir(bool fallback) override;

      void
      _user_used_ghost_code(bool success,
                            std::string const& code,
                            bool link,
                            std::string const& fail_reason) override;

      void
      _user_sent_invitation_message(bool success,
                                    std::string const& code,
                                    std::string const& method,
                                    std::string const& fail_reason) override;

      void
      _link_quota_exceeded(uint64_t size,
                           uint64_t current,
                           uint64_t total) override;

      void
      _send_to_self_limit_reached(uint64_t limit) override;

      void
      _file_transfer_limit_reached(uint64_t limit,
                                   uint64_t transfer_size) override;

    /// Implementations of UI metrics.
    private:
      void
      _ui(std::string const& event,
          std::string const& from,
          Additional const& additional) override;


    /// Private helper functions.
    private:
      std::string
      _key_str(JSONKey k);

      std::string
      _status_string(bool success);

      std::string
      _transaction_status_str(infinit::oracles::Transaction::Status status);

      std::string
      _transfer_method_str(TransferMethod method);

      std::string
      _transfer_exit_reason_str(TransferExitReason method);

      std::string
      _transaction_type_str(TransactionType type);
    /// Private attributes.
    private:
      ELLE_ATTRIBUTE(std::string, base_url);
      ELLE_ATTRIBUTE(std::string, transaction_dest);
      ELLE_ATTRIBUTE(std::string, user_dest);
      ELLE_ATTRIBUTE(std::string, ui_dest);
      ELLE_ATTRIBUTE_R(reactor::http::StatusCode, expected_status);
    };
  }
}

#endif // INFINIT_METRICS_INFINIT_REPORTER_HH
