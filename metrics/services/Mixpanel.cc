#include "Mixpanel.hh"

#include <curly/curly.hh>

#include <elle/format/base64.hh>
#include <elle/format/json/Dictionary.hh>

#include <fstream>

#include <metrics/Reporter.hh>

ELLE_LOG_COMPONENT("metrics.services.Mixpanel");

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
        return "metric_from";
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
      case Key::who_connected:
        return "who_connected";
      case Key::who_ended:
        return "who_ended";
      }
      return "unknown_key";
    }

    Mixpanel::Mixpanel(std::string const& pkey,
                       Service::Info const& info):
      Service{pkey, info}
    {}

    void
    Mixpanel::_send(TimeMetricPair metric)
    {
      if (this->info().tracking_id.empty())
        return;
      ELLE_TRACE("sending metric %s", metric);

      std::map<std::string, std::string> properties;
      properties["distinct_id"] = this->pkey();
      properties["token"] = this->info().tracking_id;

      for (Metric::value_type val: metric.second)
      {
        if (val.first != Key::tag)
          properties[key_string(val.first)] = val.second;
      }

      elle::format::json::Dictionary json_metric;

      json_metric[key_string(Key::tag)] = metric.second.at(Key::tag);
      json_metric["properties"] = properties;

      std::stringstream json_stream;
      json_metric.repr(json_stream);

      std::stringstream b64_metric;
      {
        elle::format::base64::Stream base64(b64_metric);
        {
          auto const chunk_size = 16;
          char chunk[chunk_size + 1];
          chunk[chunk_size] = 0;
          while (!json_stream.eof())
          {
            json_stream.read(chunk, chunk_size);
            base64.write(chunk, json_stream.gcount());
            base64.flush();
          }
        }
      }

      elle::Request request = this->_server->request("GET", "/track");
      request
        .user_agent(metrics::Reporter::user_agent)
        .parameter("data", b64_metric.str());

      static std::ofstream null{"/dev/null"};
      auto rc = curly::make_get();

      rc.option(CURLOPT_DEBUGFUNCTION, &Service::_curl_debug_callback);
      rc.option(CURLOPT_DEBUGDATA, this);
      rc.option(CURLOPT_TIMEOUT, 15);
      rc.user_agent(metrics::Reporter::user_agent);
      rc.url(elle::sprintf("http://%s:%d%s",
                           this->info().host,
                           this->info().port,
                           request.url()));
      rc.output(null);
      curly::request r(std::move(rc));
    }

    std::string
    Mixpanel::format_event_name(std::string const& name)
    {
      return boost::replace_all_copy(name, ".", " ");
    }
  }
}