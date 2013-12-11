#include <fstream>

#include <boost/algorithm/string/replace.hpp>

#include <elle/format/hexadecimal.hh>

#include <reactor/scheduler.hh>
#include <reactor/http/Request.hh>

#include <cryptography/Digest.hh>
#include <cryptography/Plain.hh>
#include <cryptography/oneway.hh>

#include <metrics/Reporter.hh>
#include <metrics/services/Google.hh>

ELLE_LOG_COMPONENT("metrics.google.Service");

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
        return "cm10";
      case Key::author:
        return "cm8";
      case Key::count:
        return "cm2";
      case Key::duration:
        return "cm12";
      case Key::height:
        return "cm4";
      case Key::input:
        return "cm6";
      case Key::network:
        return "cm11";
      case Key::panel:
        return "cm7";
      case Key::session:
        return "cs";
      case Key::size:
        return "cm1";
      case Key::status:
        return "cd1";
      case Key::step:
        return "cm9";
      case Key::tag:
        return "cd";
      case Key::timestamp:
        return "cm5";
      case Key::value:
        return "cd2";
      case Key::width:
        return "cm3";
      case Key::sender_online:
        return "cm12";
      case Key::recipient_online:
        return "cm13";
      case Key::source:
        return "cm14";
      case Key::method:
        return "cm19";
      // Not added to Google Analytics. Used for Mixpanel.
      case Key::connection_method:
        return "cm2000";
      case Key::file_count:
        return "cm2000";
      case Key::file_size:
        return "cm2000";
      case Key::how_ended:
        return "cm2000";
      case Key::recipient:
        return "cm2000";
      case Key::sender:
        return "cm2000";
      case Key::who:
        return "cm2000";
      case Key::who_connected:
        return "cm2000";
      case Key::who_ended:
        return "cm2000";
      default:
        return "cm2000";
      }
    }

    Google::Google(std::string const& pkey,
                   Service::Info const& info):
      Service{pkey, info},
      _hashed_pkey{_create_pkey_hash(pkey)}
    {}

    void
    Google::_send(TimeMetricPair metric)
    {
      ELLE_TRACE("sending metric %s", metric);
      // We developed our own HTTP client (GOOD TIMES!), then when we finally
      // heard reason and used Curl the HTTP client code was kept here to create
      // the request, then re-printed out in a curl request to be
      // performed. Twice the code, twice the fun ! I sometimes wish I was a
      // fishman alone at sea with nothing but the sound of waves to bother me.
      elle::Request request = this->_server->request("GET", "/collect");
      request
        .content_type("application/x-www-form-urlencoded")
        .user_agent(metrics::Reporter::user_agent)
        .parameter("", "payload_data")
        .parameter("dh", "infinit.io")
        .parameter("av", metrics::Reporter::version)
        .parameter("an", "Infinit")         // Application name.
        .parameter("t", "appview")          // Type of interaction.
        .parameter("cid", this->_hashed_pkey)
        .parameter("tid", this->info().tracking_id)
        .parameter("v", "1");               // Api version.

      typedef Metric::value_type Field;

      request.parameter(key_string(Key::tag), metric.second.at(Key::tag));

      for (Field f: metric.second)
      {
        if (f.first != Key::tag)
          request.parameter(key_string(f.first), f.second);
      };

      this->_last_sent.Current();
      auto delta = (this->_last_sent - metric.first).nanoseconds / 1000000UL;
      request.parameter("qt", std::to_string(delta));

      std::stringstream body;
      static std::ofstream null{"/dev/null"};
      body << request.body_string();

      auto url = elle::sprintf("http://%s:%d%s",
                               this->info().host,
                               this->info().port,
                               request.url());
      reactor::http::Request::Configuration cfg(15_sec);
      cfg.header_add("User-Agent", metrics::Reporter::user_agent);
      reactor::http::Request r(url,
                               reactor::http::Method::GET,
                               "application/x-www-form-urlencoded",
                               cfg);
      r << body;
      reactor::wait(r);
    }

    std::string
    Google::format_event_name(std::string const& name)
    {
      return boost::replace_all_copy(name, ".", ":");
    }

    std::string
    Google::_create_pkey_hash(std::string const& id)
    {
      ELLE_TRACE_SCOPE("creating hash from primary key %s", id);
      std::string hashed_id;
      {
        elle::WeakBuffer id_buffer{(void*)id.data(), id.length()};
        using namespace infinit::cryptography;

        Digest digest{
          oneway::hash(Plain{id_buffer}, oneway::Algorithm::sha256)};
        hashed_id = elle::format::hexadecimal::encode(digest.buffer());
      }
      ELLE_DEBUG("stage 1 hash: %s", hashed_id);

      // Google user id must have the following format:
      // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxx
      // [[:alpha:]]{8}-[[:alpha:]]{4}-[[:alpha:]]{4}-[[:alpha:]]{4}-[[:alpha:]]{12}
      if (hashed_id.length() < 32)
        hashed_id.append(32 - hashed_id.length(), '6');
      else
        hashed_id.erase(32, std::string::npos);

      hashed_id
        .insert(20, "-")
        .insert(16, "-")
        .insert(12, "-")
        .insert(8, "-");

      ELLE_DEBUG("generated google compliant hash: %s", hashed_id);
      return hashed_id;
    }
  }
}
