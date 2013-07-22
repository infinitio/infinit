#include "Google.hh"

#include <metrics/Reporter.hh>

#include <common/common.hh>

#include <cryptography/Digest.hh>
#include <cryptography/Plain.hh>
#include <cryptography/oneway.hh>

#include <elle/format/hexadecimal.hh>

#include <boost/algorithm/string/replace.hpp>

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
      }
      return "cm2000";
    }

    Google::Google(std::string const& pkey,
                   common::metrics::Info const& info):
      Service{pkey, info},
      _hashed_pkey{_create_pkey_hash(pkey)}
    {}

    void
    Google::_send(TimeMetricPair metric)
    {
      ELLE_TRACE("sending metric %s", metric);

      elle::Request request = this->_server->request("POST", "/collect");
      request
        .content_type("application/x-www-form-urlencoded")
        .user_agent(metrics::Reporter::user_agent)
        .post_field("dh", "infinit.io")
        .post_field("av", metrics::Reporter::version)
        .post_field("an", "Infinit")         // Application name.
        .post_field("t", "appview")          // Type of interaction.
        .post_field("cid", this->_hashed_pkey)
        .post_field("tid", this->info().tracking_id)
        .post_field("v", "1");               // Api version.

      typedef Metric::value_type Field;

      request.post_field(key_string(Key::tag), metric.second.at(Key::tag));

      for (Field f: metric.second)
      {
        if (f.first != Key::tag)
          request.post_field(key_string(f.first), f.second);
      };

      this->_last_sent.Current();
      auto delta = (this->_last_sent - metric.first).nanoseconds / 1000000UL;
      request.post_field("qt", std::to_string(delta));

      request.fire();
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
