#ifndef ELLE_METRICS_KISSMETRICS_HH
# define ELLE_METRICS_KISSMETRICS_HH

#include <metrics/Service.hh>

namespace metrics
{
  namespace services
  {
    class KISSmetrics:
      public Service
    {
    public:
      KISSmetrics(std::string const& pkey,
                  common::metrics::Info const& info);

    private:
      void
      _send(TimeMetricPair metric) override;

      std::string
      _format_event_name(std::string const& name) override;
    };
  }
}

#endif
