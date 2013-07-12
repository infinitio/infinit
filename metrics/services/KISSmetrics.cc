#include "KISSmetrics.hh"

#include <metrics/Reporter.hh>

#include <boost/algorithm/string/replace.hpp>

ELLE_LOG_COMPONENT("metrics.services.KISSmetrics");

namespace metrics
{
  namespace services
  {
    static const std::unordered_map<Key, std::string> keymap{
      {Key::attempt,   "attempt"},
      {Key::author,    "author"},
      {Key::count,     "count"},
      {Key::duration,  "duration"},
      {Key::height,    "height"},
      {Key::input,     "input"},
      {Key::network,   "network"},
      {Key::panel,     "panel"},
      {Key::session,   "session"},
      {Key::size,      "size"},
      {Key::status,    "status"},
      {Key::step,      "step"},
      {Key::tag,       "_n"},
      {Key::timestamp, "timestamp"},
      {Key::value,     "value"},
      {Key::width,     "witdh"},
    };

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
        .parameter(keymap.at(Key::timestamp),
                   std::to_string(metric.first.nanoseconds / 1000000UL));

      typedef Metric::value_type Field;

      request.parameter(keymap.at(Key::tag), metric.second.at(Key::tag));

      for (Field f: metric.second)
      {
        if (f.first != Key::tag)
          request.parameter(keymap.at(f.first), f.second);
      };

      request.fire();
    }

    std::string
    KISSmetrics::_format_event_name(std::string const& name)
    {
      return boost::replace_all_copy(name, ".", "_");
    }
  }
}
