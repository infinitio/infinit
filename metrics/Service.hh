#ifndef METRICS_SERVICE_HH
# define METRICS_SERVICE_HH

# include "fwd.hh"

# include "Metric.hh"

# include <common/common.hh>

# include <elle/attribute.hh>
# include <elle/HttpClient.hh>
# include <curly/curly.hh>

# include <string>
# include <memory>
# include <deque>

namespace metrics
{
  /// Abstract representation of a service.
  ///
  /// A service is used by a reporter to send metrics.
  class Service
  {
  protected:
    /// Primary key.
    ELLE_ATTRIBUTE_R(std::string, pkey);
    /// Info describing how to communicate to the service.
    ELLE_ATTRIBUTE_R(common::metrics::Info const, info);
    ELLE_ATTRIBUTE(std::deque<TimeMetricPair>, queue);

  protected: // XXX use ELLE_ATTRIBUTE when protected is available
    /// Own http client.
    std::unique_ptr<elle::HTTPClient> _server;
    /// Timestamp of the last metric sent.
    elle::utility::Time _last_sent;

    static
    int
    _curl_debug_callback(CURL* handle,
                        curl_infotype type,
                        char* what,
                        size_t what_size,
                        void* userptr);

  public:
    /// Construct a service with a primary key and its info.
    Service(std::string const& pkey,
            common::metrics::Info const& info);

    virtual
    ~Service();

  public:
    /// Send the metric to the service server.
    void
    send(TimeMetricPair metric);

  private:
    // Flush all elements in the queue.
    void
    _flush();

  protected:
    /// Service specific way to send a metric. `send()` rely on the fact that
    /// this method will throw if the metric cannot be sent.
    virtual
    void
    _send(TimeMetricPair metric) = 0;

  public:
    /// Transform a dotted name the a name compatible with the concrete service.
    virtual
    std::string
    format_event_name(std::string const& name) = 0;
  };

  /// Kind shortcut for service pointer.
  typedef std::shared_ptr<Service> ServicePtr;
}

#endif
