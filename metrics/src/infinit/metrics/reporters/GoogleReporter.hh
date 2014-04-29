#ifndef INFINIT_METRICS_GOOGLE_REPORTER_HH
# define INFINIT_METRICS_GOOGLE_REPORTER_HH

# include <elle/json/json.hh>

# include <infinit/metrics/Reporter.hh>

namespace infinit
{
  namespace metrics
  {
    enum class GoogleKey
    {
      api_version,
      application_name,
      client_id,
      client_version,
      domain,
      event,
      interaction_type,
      session,
      status,
      tracking_id,
    };

    class GoogleReporter:
      public Reporter
    {
    public:
      GoogleReporter(bool investor_reporter);

      virtual
      ~GoogleReporter() = default;

    /// Implementation of transaction metrics.
    private:
      virtual
      void
      _transaction_created(std::string const& transaction_id,
                           std::string const& sender_id,
                           std::string const& recipient_id,
                           int64_t file_count,
                           int64_t total_size,
                           uint32_t message_length,
                           bool ghost);

    /// Implementation of user metrics.
    private:
      virtual
      void
      _user_login(bool success, std::string const& info);

      virtual
      void
      _user_logout(bool success, std::string const& info);

    /// Private helper functions.
    private:
      std::string
      _create_pkey_hash(std::string const& id);

      std::string
      _key_str(GoogleKey k);

      std::string
      _make_param(bool first,
                  std::string const& parameter,
                  std::string const& value);

      void
      _send(std::unordered_map<std::string, std::string>& data);

      std::string
      _status_string(bool success);

    /// Private attributes.
    private:
      ELLE_ATTRIBUTE(std::string, base_url);
      ELLE_ATTRIBUTE(std::string, hashed_pkey);
      ELLE_ATTRIBUTE(bool, investor_reporter);
      ELLE_ATTRIBUTE(int, port);
      ELLE_ATTRIBUTE(std::string, tracking_id);
    };
  }
}

#endif // INFINIT_METRICS_GOOGLE_REPORTER_HH
