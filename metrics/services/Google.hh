#ifndef METRICS_SERVICES_GOOGLE_HH
# define METRICS_SERVICES_GOOGLE_HH

# include <metrics/Service.hh>

namespace metrics
{
  namespace services
  {
    class Google:
      public Service
    {
    public:
      std::string const _hashed_pkey;
    public:
      Google(std::string const& pkey,
             common::metrics::Info const& info);

    protected:
      void
      _send(TimeMetricPair metric) override;

      std::string
      _format_event_name(std::string const& name) override;

      static
      std::string
      _create_pkey_hash(std::string const& pkey);
    };
  }
}

#endif
