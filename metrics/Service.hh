#ifndef METRICS_SERVICE_HH
# define METRICS_SERVICE_HH

# include "fwd.hh"

# include "Metric.hh"

# include <elle/attribute.hh>
# include <elle/HttpClient.hh>

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
  public:
    struct Info
    {
      std::string const pretty_name;
      std::string const host;
      uint16_t const port;
      std::string const id_path;
      std::string const tracking_id;

      Info(std::string const& pretty_name,
           std::string const& host,
           uint16_t port,
           std::string const& id_path,
           std::string const& tracking_id):
        pretty_name(pretty_name),
        host(host),
        port(port),
        id_path(id_path),
        tracking_id(tracking_id)
      {}
    };

  protected:
    /// Primary key.
    ELLE_ATTRIBUTE_R(std::string, pkey);
    /// Info describing how to communicate to the service.
    ELLE_ATTRIBUTE_R(Info const, info);
    ELLE_ATTRIBUTE(std::deque<TimeMetricPair>, queue);

  protected: // XXX use ELLE_ATTRIBUTE when protected is available
    /// Own http client.
    /// XXX: elle::HTTPClient is deprecated but it's also a factory of
    /// HTTPRequest that are pretty easy to use.
    std::unique_ptr<elle::HTTPClient> _server;
    /// Timestamp of the last metric sent.
    elle::utility::Time _last_sent;

  public:
    /// Construct a service with a primary key and its info.
    Service(std::string const& pkey,
            Info const& info);

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
