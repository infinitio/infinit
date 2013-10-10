#include <metrics/services/Infinit.hh>

#include <chrono>

#include <curly/curly.hh>

#include <elle/format/base64.hh>
#include <elle/format/json/Dictionary.hh>

#include <fstream>

#include <metrics/Reporter.hh>

#include <version.hh>

ELLE_LOG_COMPONENT("metrics.services.Infinit");

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
      case Key::connection_method:
        return "connection_method";
      case Key::count:
        return "count";
      case Key::duration:
        return "duration";
      case Key::file_count:
        return "file_count";
      case Key::file_size:
        return "file_size";
      case Key::height:
        return "height";
      case Key::how_ended:
        return "how_ended";
      case Key::input:
        return "input";
      case Key::metric_from:
        return "user";
      case Key::network:
        return "network";
      case Key::panel:
        return "panel";
      case Key::recipient:
        return "recipient";
      case Key::sender:
        return "sender";
      case Key::session:
        return "session";
      case Key::size:
        return "size";
      case Key::status:
        return "status";
      case Key::step:
        return "step";
      case Key::tag:
        return "event";
      case Key::timestamp:
        return "timestamp";
      case Key::transaction_id:
        return "transaction";
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
      case Key::who:
        return "who";
      case Key::who_connected:
        return "who_connected";
      case Key::who_ended:
        return "who_ended";
      }
      return "unknown_key";
    }

    Infinit::Infinit(std::string const& pkey,
                     Service::Info const& info,
                     metrics::Kind service_kind):
      Service{pkey, info},
      _service_kind(service_kind)
    {}

    void
    Infinit::_send(TimeMetricPair metric)
    {
      ELLE_TRACE("sending metric %s", metric);

      elle::format::json::Dictionary json_metric;

      std::string url_resource;
      if (this->_service_kind == metrics::Kind::transaction)
      {
        json_metric["transaction"] = this->pkey();
        url_resource = "/transactions";
      }
      else if (this->_service_kind == metrics::Kind::user)
      {
        json_metric["user"] = this->pkey();
        url_resource = "/users";
      }

      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

      json_metric["timestamp"] = timestamp;
      json_metric["version"] = INFINIT_VERSION;

      for (Metric::value_type field: metric.second)
        json_metric[key_string(field.first)] = field.second;

      std::stringstream null;

      std::stringstream input;
      json_metric.repr(input);

      auto rc = curly::make_post();
      rc.option(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
      rc.option(CURLOPT_POSTFIELDSIZE, input.str().size());
      rc.option(CURLOPT_DEBUGFUNCTION, &Service::_curl_debug_callback);
      rc.option(CURLOPT_DEBUGDATA, this);
      rc.option(CURLOPT_TIMEOUT, 15);
      rc.user_agent(metrics::Reporter::user_agent);
      rc.headers({
        {"Content-Type", "application/json"}
      });
      rc.url(elle::sprintf("http://%s:%d%s",
                           this->info().host,
                           this->info().port,
                           url_resource));
      rc.input(input);
      rc.output(null);

      curly::request r(std::move(rc));
    }

    std::string
    Infinit::format_event_name(std::string const& name)
    {
      return boost::replace_all_copy(name, ".", " ");
    }
  }
}