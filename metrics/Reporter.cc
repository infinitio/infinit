#include "Reporter.hh"

#include <version.hh>
#include <common/common.hh>

#include <reactor/scheduler.hh>

#include <elle/log.hh>
#include <elle/threading/Monitor.hh>

#include <fstream>
#include <thread>

ELLE_LOG_COMPONENT("metrics.Reporter");

namespace metrics
{
  std::string Reporter::version =
    elle::sprintf("%s.%s", INFINIT_VERSION_MAJOR, INFINIT_VERSION_MINOR);
  std::string Reporter::user_agent = elle::sprintf("Infinit/%s (%s)",
                                                   Reporter::version,
#ifdef INFINIT_LINUX
                                                "Linux x86_64");
#elif INFINIT_MACOSX
             // XXX[10.7: should adapt to any MacOS X version]
                                                "Mac OS X 10.7");
#else
# warning "machine not supported"
#endif

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

    Impl():
      scheduler{},
      keep_alive{},
      thread{nullptr},
      fallback_stream{common::metrics::fallback_path()},
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
          // Fill services if some are missing.
          while (services.size() < this->service_factories.size())
            services.emplace_back(
              this->service_factories[services.size()](pkey));
          return services;
        });
    }
  };

  Reporter::Reporter():
    _this{new Impl{}}
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
    TimeMetricPair timed_metric{
      elle::utility::Time::current(),
      std::move(metric),
    };

    ELLE_TRACE("storing new metric %s: %s = %s", pkey, event_name, timed_metric);

    auto& services = _this->get_services(pkey);

    for (auto& service: services)
    {
      _this->scheduler.io_service().post(
        [&service, pkey, timed_metric, event_name, this] {
          try
          {
            TimeMetricPair cpy{timed_metric};
            cpy.second.emplace(
              Key::tag,
              service->_format_event_name(event_name));
            service->_send(cpy);
          }
          catch (...)
          {
            ELLE_ERR("error while storing timed_metric %s: %s",
                     timed_metric,
                     elle::exception_string());
           this->_fallback(pkey, event_name, timed_metric);
          }
        });
    }
  }

  void
  Reporter::_fallback(std::string const& /*pkey*/,
                      std::string const& /*event_name*/,
                      TimeMetricPair const& /*metric*/)
  {
    // XXX do fallback
  }
}
