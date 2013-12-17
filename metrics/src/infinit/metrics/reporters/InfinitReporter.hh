#ifndef INFINIT_METRICS_INFINIT_REPORTER_HH
# define INFINIT_METRICS_INFINIT_REPORTER_HH

# include <elle/json/json.hh>

# include <infinit/metrics/Reporter.hh>

namespace infinit
{
  namespace metrics
  {
    enum class InfinitKey
    {
      connection_method,
      event,
      fail_reason,
      file_count,
      how_ended,
      message_length,
      metric_sender_id,
      recipient_id,
      sender_id,
      status,
      timestamp,
      total_size,
      transaction_id,
      user_agent,
      version,
      who,
    };

    class InfinitReporter:
      public Reporter
    {
    public:
      InfinitReporter();

      virtual
      ~InfinitReporter() = default;

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
                           uint32_t file_count,
                           uint64_t total_size,
                           uint32_t message_length);

      virtual
      void
      _transaction_ended(std::string const& transaction_id,
                         infinit::oracles::Transaction::Status status,
                         std::string const& info);

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

    /// Private helper functions.
    private:
      std::string
      _key_str(InfinitKey k);

      void
      _send(std::string const& destination, elle::json::Object data);

      std::string
      _status_string(bool success);

      std::string
      _transaction_status_str(infinit::oracles::Transaction::Status status);

    /// Private attributes.
    private:
      ELLE_ATTRIBUTE(std::string, base_url);
      ELLE_ATTRIBUTE(int, port);
      ELLE_ATTRIBUTE(std::string, transaction_dest);
      ELLE_ATTRIBUTE(std::string, user_dest);
    };
  }
}

#endif // INFINIT_METRICS_INFINIT_REPORTER_HH