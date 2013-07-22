#include "KISSmetrics.hh"

#include <metrics/Reporter.hh>

#include <boost/algorithm/string/replace.hpp>

ELLE_LOG_COMPONENT("metrics.services.KISSmetrics");

namespace metrics
{
  namespace services
  {
    static
    std::string
    key_string(metrics::Key const k)
    {
      switch (k)
      {
      case Key::attempt:
        return "attempt";
      case Key::author:
        return "author";
      case Key::count:
        return "count";
      case Key::duration:
        return "duration";
      case Key::height:
        return "height";
      case Key::input:
        return "input";
      case Key::network:
        return "network";
      case Key::panel:
        return "panel";
      case Key::session:
        return "session";
      case Key::size:
        return "size";
      case Key::status:
        return "status";
      case Key::step:
        return "step";
      case Key::tag:
        return "_n";
      case Key::timestamp:
        return "timestamp";
      case Key::value:
        return "value";
      case Key::width:
        return "witdh";
      case Key::sender_online:
        return "sender_online";
      case Key::recipient_online:
        return "recipient_online";
      case Key::source:
        return "source";
      case Key::method:
        return "method";
      }
      return "unknown_key";
    }

    //- KISSmetrics --------------------------------------------------------------
    KISSmetrics::KISSmetrics(std::string const& pkey,
                             common::metrics::Info const& info):
      Service{pkey, info}
    {}

    void
    KISSmetrics::_send(TimeMetricPair metric)
    {
      ELLE_TRACE("sending metric %s", metric);

      // XXX copy constructor with should great idea, cause request base here
      // is always the same, so it could be static.
      elle::Request request = this->_server->request("GET", "/e");
      request
        .user_agent(metrics::Reporter::user_agent)
        .parameter("version", metrics::Reporter::version)
        .parameter("app_name", "Infinit")
        .parameter("_p", this->pkey())
        .parameter("_k", this->info().tracking_id)
        .parameter("version", "1")
        .parameter(key_string(Key::timestamp),
                   std::to_string(metric.first.nanoseconds / 1000000UL));

      typedef Metric::value_type Field;

      request.parameter(key_string(Key::tag), metric.second.at(Key::tag));

      for (Field f: metric.second)
      {
        if (f.first != Key::tag)
          request.parameter(key_string(f.first), f.second);
      };

      request.fire();
    }

    std::string
    KISSmetrics::format_event_name(std::string const& name)
    {
      return boost::replace_all_copy(name, ".", "_");
    }
  }
}
