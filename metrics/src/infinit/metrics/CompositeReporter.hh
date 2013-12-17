#ifndef INFINIT_METRICS_COMPOSITE_REPORTER_HH
# define INFINIT_METRICS_COMPOSITE_REPORTER_HH

# include <vector>

# include <infinit/metrics/Reporter.hh>

namespace infinit
{
  namespace metrics
  {
    class CompositeReporter:
      public Reporter
    {
    public:
      CompositeReporter();

      virtual
      ~CompositeReporter() = default;

      virtual
      void
      start();

      virtual
      void
      stop();

    /// Reporter management.
    public:
      void
      add_reporter(std::unique_ptr<Reporter>&& reporter);

    /// Transaction metrics.
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

    /// User metrics.
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
      _user_register(bool success, std::string const& info);

      virtual
      void
      _user_unfavorite(std::string const& user_id);

    /// Dispatch metrics.
    private:
      void
      _dispatch(std::function<void(Reporter*)> fn);

    /// Private attributes.
    private:
      ELLE_ATTRIBUTE(std::vector<std::unique_ptr<Reporter>>,
                     reporters);
    };
  }
}

#endif // INFINIT_METRICS_COMPOSITE_REPORTER_HH
