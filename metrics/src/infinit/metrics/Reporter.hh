#ifndef INFINIT_METRICS_REPORTER_HH
# define INFINIT_METRICS_REPORTER_HH

# include <queue>
# include <string>

# include <reactor/Barrier.hh>
# include <reactor/signal.hh>
# include <reactor/thread.hh>
# include <reactor/http/StatusCode.hh>
# include <reactor/network/proxy.hh>

# include <infinit/oracles/Transaction.hh>

namespace infinit
{
  namespace metrics
  {
    enum TransferMethod
    {
      TransferMethodP2P,
      TransferMethodCloud,
      TransferMethodGhostCloud,
    };

    enum TransferExitReason
    {
      TransferExitReasonFinished,
      TransferExitReasonExhausted, // no more data from that source
      TransferExitReasonError, // specific error
      TransferExitReasonTerminated, // terminated by fsm state change
      TransferExitReasonUnknown, // not properly caught reason should not happen
    };

    enum TransactionType
    {
      LinkTransaction,
      PeerTransaction,
    };

    class CompositeReporter;
    /// Abstract metrics reporter.
    class Reporter:
      public elle::Printable
    {
    friend CompositeReporter;
      typedef std::function<void()> Metric;
    public:
      typedef std::unordered_map<std::string, std::string> Additional;

    public:
      Reporter(std::string const& name);

      virtual
      ~Reporter();

      void
      start();

      virtual
      void
      stop();

    /// Public attributes.
    public:
      ELLE_ATTRIBUTE_R(std::string, name);
      ELLE_ATTRIBUTE_R(reactor::network::Proxy, proxy);

    /// Proxy management.
    /// Overridden in CompositeReporter to ensure that all reporters have their
    /// proxy set.
    public:
      virtual
      void
      proxy(reactor::network::Proxy const& proxy);

    /// Setter for metric sender id and device id.
    public:
      static
      void
      metric_sender_id(std::string const& metric_sender_id);

      static
      void
      metric_device_id(std::string const& device_id);

      static
      void
      metric_features(std::unordered_map<std::string, std::string> const& features);

    /// Transaction metrics.
    public:
      void
      transaction_accepted(std::string const& transaction_id,
                           bool onboarding);

      void
      transaction_connected(std::string const& transaction_id,
                            std::string const& connection_method,
                            int attempt);

      void
      link_transaction_created(std::string const& transaction_id,
                               std::string const& sender_id,
                               int64_t file_count,
                               int64_t total_size,
                               uint32_t message_length,
                               bool screenshot,
                               std::vector<std::string> extensions,
                               bool onboarding = false);

      void
      peer_transaction_created(std::string const& transaction_id,
                               std::string const& sender_id,
                               std::string const& recipient_id,
                               int64_t file_count,
                               int64_t total_size,
                               uint32_t message_length,
                               bool ghost,
                               bool onboarding,
                               std::vector<std::string> extensions);

      void
      transaction_ended(std::string const& transaction_id,
                        infinit::oracles::Transaction::Status status,
                        std::string const& info,
                        bool onboarding,
                        bool caused_by_user = false);

      void
      transaction_deleted(std::string const& transaction_id);

      /** entering a state that will effectively transfer data
      * @param initialization_time: time in seconds between entering state
      *        and effectively sending/receiving the first bytes
      */
      void
      transaction_transfer_begin(std::string const& transaction_id,
                                 TransferMethod method,
                                 float initialization_time,
                                 int attempt=0);

      void
      transaction_transfer_end(std::string const& transaction_id,
                               TransferMethod method,
                               float duration,
                               uint64_t bytes_transfered,
                               TransferExitReason reason,
                               std::string const& message,
                               int attempt=0);

      void
      aws_error(std::string const& transaction_id,
                std::string const& operation,
                std::string const& url,
                unsigned int attempt,
                int http_status,
                std::string const& aws_error_code,
                std::string const& message);

      void
      link_quota_exceeded(uint64_t size, uint64_t current, uint64_t quota);

    /// Transaction metrics implementation.
    protected:
      virtual
      void
      _transaction_accepted(std::string const& transaction_id,
                            bool onboarding);

      virtual
      void
      _transaction_connected(std::string const& transaction_id,
                             std::string const& connection_method,
                             int attempt);

      virtual
      void
      _link_transaction_created(std::string const& transaction_id,
                                std::string const& sender_id,
                                int64_t file_count,
                                int64_t total_size,
                                uint32_t message_length,
                                bool screenshot,
                                std::vector<std::string> extensions,
                                bool onboarding);

      virtual
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
                                );

      virtual
      void
      _transaction_ended(std::string const& transaction_id,
                         infinit::oracles::Transaction::Status status,
                         std::string const& info,
                         bool onboarding,
                         bool caused_by_user);

      virtual
      void
      _transaction_deleted(std::string const& transaction_id);

      virtual
      void
      _transaction_transfer_begin(std::string const& transaction_id,
                                 TransferMethod method,
                                 float initialization_time,
                                 int attempt);

      virtual
      void
      _transaction_transfer_end(std::string const& transaction_id,
                               TransferMethod method,
                               float duration,
                               uint64_t bytes_transfered,
                               TransferExitReason reason,
                               std::string const& message,
                               int attempt);

      virtual
      void
      _aws_error(std::string const& transaction_id,
                 std::string const& operation,
                 std::string const& url,
                 unsigned int attempt,
                 int http_status,
                 std::string const& aws_error_code,
                 std::string const& message);

      virtual
      void
      _link_quota_exceeded(uint64_t size, uint64_t current, uint64_t quota);

    /// User metrics.
    public:
      void
      user_favorite(std::string const& user_id);

      void
      user_login(bool success,
                 std::string const& info);

      void
      facebook_connect(bool success, std::string const& info);

      void
      user_logout(bool success,
                  std::string const& info);

      void
      user_register(
        bool success,
        std::string const& info,
        std::string const& with = "",
        boost::optional<std::string> ghost_code = boost::none,
        boost::optional<std::string> referral_code = boost::none);

      void
      user_unfavorite(std::string const& user_id);

      void
      user_heartbeat();

      void
      user_first_launch();

      void
      user_proxy(reactor::network::ProxyType proxy_type);

      void
      user_crashed();

      void
      user_changed_download_dir(bool fallback);

      void
      user_sent_invitation_message(bool success,
                                   std::string const& code,
                                   std::string const& method,
                                   std::string const& fail_reason);

      void
      user_used_ghost_code(bool success,
                           std::string const& code,
                           bool link,
                           std::string const& fail_reason);

    /// User metrics implementation.
    protected:
      virtual
      void
      _user_favorite(std::string const& user_id);

      virtual
      void
      _user_login(bool success,
                  std::string const& info);

      virtual
      void
      _facebook_connect(bool success,
                        std::string const& info);

      virtual
      void
      _user_logout(bool success,
                   std::string const& info);

      virtual
      void
      _user_register(bool success,
                     std::string const& info,
                     std::string const& with,
                     boost::optional<std::string> ghost_code,
                     boost::optional<std::string> referral_code);

      virtual
      void
      _user_unfavorite(std::string const& user_id);

      virtual
      void
      _user_heartbeat();

      virtual
      void
      _user_first_launch();

      virtual
      void
      _user_proxy(reactor::network::ProxyType proxy_type);

      virtual
      void
      _user_crashed();

      virtual
      void
      _user_changed_download_dir(bool fallback);

      virtual
      void
      _user_used_ghost_code(bool success,
                            std::string const& code,
                            bool link,
                            std::string const& fail_reason);

      virtual
      void
      _user_sent_invitation_message(bool success,
                                    std::string const& code,
                                    std::string const& method,
                                    std::string const& fail_reason);

      /// UI metrics.
    public:
      void
      ui(std::string const& event,
         std::string const& from,
         Additional const& additional);

      /// UI metrics implementation.
    protected:
      virtual
      void
      _ui(std::string const& event,
          std::string const& from,
          Additional const& additional);

    /// Queue handling.
    private:
      void
      _push(Metric const& func);

      void
      _poll();

    /// Protected variable setters.
    protected:
      void
      name(std::string name);

    /// Static class attribute accessors.
    protected:
      static
      std::string
      client_version();

      static
      std::string
      metric_sender_id();

      static
      std::string
      metric_device_id();

      static
      std::unordered_map<std::string, std::string>const&
      metric_features();

      static
      std::string
      user_agent();

    /// Static class attributes.
    private:
      static
      std::string _client_version;

      static
      std::string _metric_sender_id;

      static
      std::string _metric_device_id;

      static
      std::unordered_map<std::string, std::string>
      _metric_features;

      static
      std::string _user_agent;

    /// Private attributes.
    private:
      ELLE_ATTRIBUTE(reactor::Barrier, metric_available);
      ELLE_ATTRIBUTE(std::queue<Metric>, metric_queue);
      ELLE_ATTRIBUTE(reactor::Signal, metric_queue_empty);
      ELLE_ATTRIBUTE(bool, no_metrics);
      ELLE_ATTRIBUTE(std::unique_ptr<reactor::Thread>, poll_thread);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;
    };
  }
}

#endif // METRICS_REPORTER_HH
