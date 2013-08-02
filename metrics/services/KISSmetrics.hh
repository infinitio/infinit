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
                  Service::Info const& info);

    protected:
      void
      _send(TimeMetricPair metric) override;

      std::string
      format_event_name(std::string const& name) override;
    };
  }
}

#endif
