#ifndef ELLE_METRICS_INFINIT_HH
# define ELLE_METRICS_INFINIT_HH

# include <metrics/Kind.hh>
# include <metrics/Service.hh>

namespace metrics
{
  namespace services
  {
    class Infinit:
      public Service
      {
      private:
        ELLE_ATTRIBUTE(metrics::Kind, service_kind);

      public:
        Infinit(std::string const& pkey,
                Service::Info const& info,
                metrics::Kind service_kind);
      protected:
        void
        _send(TimeMetricPair metric) override;

        std::string
        format_event_name(std::string const& name) override;

      };
  }
}

#endif // ifndef ELLE_METRICS_INFINIT_HH