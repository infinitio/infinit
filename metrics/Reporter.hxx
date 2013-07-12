#ifndef METRICS_REPORTER_HXX
# define METRICS_REPORTER_HXX

# include "Reporter.hh"

namespace metrics
{
  Reporter::Proxy::Proxy(Reporter& reporter,
                         std::string const& pkey):
    reporter(reporter),
    pkey{pkey}
  {}

  void
  Reporter::Proxy::store(std::string const& name,
                         Metric metric)
  {
    this->reporter._store(this->pkey, name, metric); //std::move(metric));
  }

  namespace details
  {
    template <typename S, typename... Args>
    ServicePtr
    make_service_instance(std::string const& pkey,
                          Args const&... args)
    {
      return ServicePtr{new S(pkey, args...)};
    }
  }

  template <typename S, typename... Args>
  void
  Reporter::add_service_class(Args const&... args)
  {
    this->add_service_factory(
      std::bind(&details::make_service_instance<S, Args...>,
                std::placeholders::_1,
                args...));
  }
}

#endif
