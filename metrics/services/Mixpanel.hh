#ifndef ELLE_METRICS_MIXPANEL_HH
# define ELLE_METRICS_MIXPANEL_HH

# include <metrics/Service.hh>

namespace metrics
{
  namespace services
  {
    class Mixpanel:
      public Service
      {
      public:
        Mixpanel(std::string const& pkey,
                 Service::Info const& info);
      protected:
        void
        _send(TimeMetricPair metric) override;

        std::string
        format_event_name(std::string const& name) override;
      };
  }
}

#endif // ifndef ELLE_METRICS_MIXPANEL_HH