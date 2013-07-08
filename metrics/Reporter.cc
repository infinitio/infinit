#include <common/common.hh>

#include <elle/Buffer.hh>
#include <elle/container/map.hh>
#include <elle/format/hexadecimal.hh>
#include <elle/serialize/BinaryArchive.hh>
#include <elle/serialize/ListSerializer.hxx>
#include <elle/serialize/PairSerializer.hxx>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>
#include <elle/utility/Time.hh>

#include <version.hh>
#include "Reporter.hh"

#include <fstream>
#include <iostream>
#include <string>
#include <regex>
#include <thread>
#include <mutex>

ELLE_LOG_COMPONENT("elle.metrics.Reporter");

namespace elle
{
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
    /*----.
    | Key |
    `----*/
    std::ostream&
    operator <<(std::ostream& out,
                Key k)
    {
      static std::map<Key, std::string> const map{
        {Key::attempt,    "attempt"},
        {Key::author,     "author"},
        {Key::count,      "count"},
        {Key::duration,   "duration"},
        {Key::height,     "height"},
        {Key::input,      "input"},
        {Key::network,    "network"},
        {Key::panel,      "panel"},
        {Key::session,    "session"},
        {Key::size,       "size"},
        {Key::status,     "status"},
        {Key::step,       "step"},
        {Key::tag,        "tag"},
        {Key::timestamp,  "timestamp"},
        {Key::value,      "value"},
        {Key::width,      "width"},};
      return out << map.at(k);
    }

    /*--------.
    | Service |
    `--------*/
    Reporter::Service::Service(std::string const& host,
                               uint16_t port,
                               std::string const& user,
                               std::string const& pretty_name)
      : _user_id{user}
      , _server{new elle::HTTPClient{host, port, Reporter::user_agent}}
      , _name{pretty_name}
    {}

    Reporter::Service::~Service()
    {}

    void
    Reporter::Service::update_user(std::string const& user)
    {
      this->_user_id = user;
    }

    /*---------.
    | Reporter |
    `---------*/
    Reporter::Reporter()
      : _flusher_sched{}
      , _fallback_stream{common::metrics::fallback_path()}
    {
      this->_keep_alive.reset(
        new boost::asio::io_service::work(_flusher_sched.io_service()));
    }

    Reporter::~Reporter()
    {
      this->_keep_alive.reset();
      if (this->_run_thread && this->_run_thread->joinable())
        this->_run_thread->join();
    }

    void
    Reporter::start()
    {
      this->_run_thread.reset(new std::thread{[this] { this->_flusher_sched.run(); }});
    }

    void
    Reporter::store(std::string const& caller, Metric const& metric)
    {
      ELLE_TRACE("Storing new metric %s", caller);

      // Note that if we want the ability to use initializer list for metric,
      // we can't declare it as non const..
      Metric& m = const_cast<Metric &>(metric);
      m.emplace(Key::tag, caller);

      this->store(TimeMetricPair(elle::utility::Time::current(), m));
    }

    void
    Reporter::store(std::string const& caller)
    {
      this->store(caller, Metric{});
    }

    void
    Reporter::store(std::string const& name,
                    Key const& key,
                    std::string const& value)
    {
      Metric metric;
      metric.emplace(key, value);
      this->store(name, metric);
    }

    void
    Reporter::store(TimeMetricPair const& metric)
    {
      ELLE_TRACE("metric: %s", metric);

      this->_services(
        [metric, this] (ServicesMap& services) -> void
        {
          for (auto& service: services)
          {
            this->_flusher_sched.io_service().post([&service, metric, this] {
                try
                {
                  service.second->_send(metric);
                }
                catch (std::runtime_error const& e)
                {
                  ELLE_ERR("error while storing metric %s %s", metric, e.what());
                  // XXX: Fallback is not threadsafe.
                  this->_fallback(service.first, metric);
                }
              });
          }
        });
    }

    void
    Reporter::_fallback(std::string const& name, TimeMetricPair const& metric)
    {
      (void)name;
      (void)metric;
    }

    void
    Reporter::add_service(std::unique_ptr<Service> service)
    {
      this->_services(
        [&] (ServicesMap& services) -> void
        {
          services.emplace(service->name(), std::move(service));
        });
    }

    Reporter::Service&
    Reporter::_service(std::string const& name)
    {
      return this->_services(
        [&] (ServicesMap& services) -> Reporter::Service&
        {
          return *services.at(name);
        });
    }

    void
    Reporter::update_user(std::string const& user)
    {
      this->_services(
        [&] (ServicesMap& services) -> void
        {
          for (auto& serv: services)
            serv.second->update_user(user);
        });
    }

    Reporter&
    reporter()
    {
      static Reporter reporter;

      return reporter;
    }
  } // End of metric
} // End of elle
