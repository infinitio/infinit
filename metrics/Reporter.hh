#ifndef METRICS_REPORTER_HH
# define METRICS_REPORTER_HH

# include "Service.hh"

# include <memory>
# include <string>

namespace metrics
{
  /// Aggregate any number of services and use them to send metrics.
  class Reporter
  {
  public:
    static std::string version;
    static std::string user_agent;
    static std::string tag_placeholder;

  public:
    Reporter();
    ~Reporter();

  private:
    // non copiable
    Reporter(Reporter const&) = delete;
    Reporter&
    operator =(Reporter const&) = delete;

  public:
    /// Start publishing.
    void
    start();

  public:
    /// Add a service type to be constructed when required.
    template <typename S, typename... Args>
    void
    add_service_class(Args const&... args);

    void
    add_service_factory(std::function<ServicePtr(std::string const&)> factory);

  public:
    struct Proxy;
    friend struct Proxy;
  public:
    /// Access to the store method for a specific primary key.
    Proxy&
    operator [](std::string const& pkey);

  protected:
    // Enqueue data.
    void
    _store(std::string const& pkey,
           std::string const& event_name,
           Metric metric);

  protected:
    struct Impl;
    std::unique_ptr<Impl> _this;
  };

  /// Proxy object used in by the Reporter::operator[] to store metrics related
  /// to a specific primary key.
  struct Reporter::Proxy
  {
    Reporter& reporter;
    std::string const pkey;

    explicit inline
    Proxy(Reporter& reporter,
          std::string const& pkey);

    void
    inline
    store(std::string const& name,
          Metric metric = {});
  };
}

# include "Reporter.hxx"

#endif
