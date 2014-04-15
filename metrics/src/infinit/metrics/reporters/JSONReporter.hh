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
      bytes_transfered,
      connection_method,
      duration,
      event,
      exit_reason,
      fail_reason,
      file_count,
      initialization_time,
      how_ended,
      message,
      message_length,
      metric_sender_id,
      recipient_id,
      sender_id,
      status,
      timestamp,
      transfer_method,
      total_size,
      transaction_id,
      user_agent,
      version,
      who,
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
      virtual
      void
      _transaction_accepted(std::string const& transaction_id);

      virtual
      void
      _transaction_connected(std::string const& transaction_id,
                             std::string const& connection_method);

      virtual
      void
      _transaction_created(std::string const& transaction_id,
                           std::string const& sender_id,
                           std::string const& recipient_id,
                           int64_t file_count,
                           int64_t total_size,
                           uint32_t message_length);

      virtual
      void
      _transaction_ended(std::string const& transaction_id,
                         infinit::oracles::Transaction::Status status,
                         std::string const& info);



      virtual
      void
      _transaction_transfer_begin(std::string const& transaction_id,
                                 TransferMethod method,
                                 float initialization_time);

      virtual
      void
      _transaction_transfer_end(std::string const& transaction_id,
                               TransferMethod method,
                               float duration,
                               uint64_t bytes_transfered,
                               TransferExitReason reason,
                               std::string const& message);

    /// Implementation of user metrics.
    private:
      virtual
      void
      _user_favorite(std::string const& user_id);

      virtual
      void
      _user_login(bool success, std::string const& info);

      virtual
      void
      _user_logout(bool success, std::string const& info);

      virtual
      void
      _user_unfavorite(std::string const& user_id);

      virtual
      void
      _user_heartbeat();

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
    /// Private attributes.
    private:
      ELLE_ATTRIBUTE(std::string, base_url);
      ELLE_ATTRIBUTE(std::string, transaction_dest);
      ELLE_ATTRIBUTE(std::string, user_dest);
      ELLE_ATTRIBUTE_R(reactor::http::StatusCode, expected_status);
    };
  }
}

#endif // INFINIT_METRICS_INFINIT_REPORTER_HH
