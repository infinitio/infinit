#include "Reporter.hh"

#include <version.hh>

#include <reactor/scheduler.hh>

#include <elle/log.hh>
#include <elle/os/getenv.hh>
#include <elle/threading/Monitor.hh>

#include <fstream>
#include <thread>

ELLE_LOG_COMPONENT("metrics.Reporter");

namespace metrics
{
  std::string Reporter::version = INFINIT_VERSION;
  std::string Reporter::user_agent = elle::sprintf(
    "Infinit/%s (%s)",
    Reporter::version,
#ifdef INFINIT_LINUX
    "Linux"
#elif INFINIT_MACOSX
    "OS X"
#else
# warning "machine not supported"
#endif
  );

  struct Reporter::Impl
  {
    typedef std::function<ServicePtr(std::string const&)> ServiceFactory;
    typedef std::vector<ServicePtr> ServiceArray;
    typedef std::unordered_map<std::string, ServiceArray> ServiceMap;
    typedef std::unordered_map<std::string, Proxy> ProxyMap;

    reactor::Scheduler scheduler;
    std::unique_ptr<boost::asio::io_service::work> keep_alive;
    std::unique_ptr<std::thread> thread;
    std::ofstream fallback_stream;
    std::vector<ServiceFactory> service_factories;
    elle::threading::Monitor<ServiceMap> services;
    elle::threading::Monitor<ProxyMap> proxies;

    Impl(std::string const& fallback_path):
      scheduler{},
      keep_alive{},
      thread{nullptr},
      fallback_stream{fallback_path},
      service_factories{},
      services{},
      proxies{}
    {}

    ServiceArray&
    get_services(std::string const& pkey)
    {
      return this->services(
        [&] (ServiceMap& map) -> ServiceArray& {
          ServiceArray& services = map[pkey];
          while (services.size() < service_factories.size())
            services.emplace_back(
              this->service_factories[services.size()](pkey));
          return services;
        });
    }
  };

  Reporter::Reporter(std::string const& fallback_path):
    _this{new Impl{fallback_path}}
  {
    _this->keep_alive.reset(
      new boost::asio::io_service::work(_this->scheduler.io_service()));
  }

  Reporter::~Reporter()
  {
    _this->keep_alive.reset();
    if (_this->thread != nullptr && _this->thread->joinable())
      _this->thread->join();
  }

  void
  Reporter::start()
  {
    _this->thread.reset(new std::thread{[&] { _this->scheduler.run(); }});
  }

  void
  Reporter::add_service_factory(
    std::function<ServicePtr(std::string const&)> factory)
  {
    _this->service_factories.push_back(factory);
  }

  Reporter::Proxy&
  Reporter::operator [](std::string const& pkey)
  {
    return _this->proxies(
      [&] (Impl::ProxyMap& proxies) -> Proxy& {
        auto it = proxies.find(pkey);
        if (it == proxies.end())
        {
          proxies.emplace(pkey, Proxy{*this, pkey});
          return proxies.at(pkey);
        }
        return it->second;
      });
  }

  void
  Reporter::_store(std::string const& pkey,
                   std::string const& event_name,
                   Metric metric)
  {
    if (elle::os::in_env("INFINIT_NO_METRICS"))
      return;

    TimeMetricPair timed_metric{
      elle::utility::Time::current(),
      std::move(metric),
    };

    ELLE_TRACE("storing new metric %s: %s = %s", pkey, event_name, timed_metric);

    auto& services = _this->get_services(pkey);

    for (auto& service: services)
    {
      _this->scheduler.io_service().post(
        // Note: `service` is a shared_ptr, copyied by the lambda
        [service, pkey, timed_metric, event_name, this] {
          TimeMetricPair cpy{timed_metric};
          cpy.second.emplace(
            Key::tag,
            service->format_event_name(event_name));
          try
          {
            service->send(cpy);
          }
          catch (std::exception const&)
          {
            ELLE_ERR("error while storing timed_metric %s: %s",
                     cpy,
                     elle::exception_string());
          }
        });
    }
  }
}
