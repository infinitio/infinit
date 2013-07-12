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
    static const std::unordered_map<metrics::Key, std::string> keymap{
      {Key::attempt,   "cm10"},
      {Key::author,    "cm8"},
      {Key::count,     "cm2"},
      {Key::duration,  "cm12"},
      {Key::height,    "cm4"},
      {Key::input,     "cm6"},
      {Key::network,   "cm11"},
      {Key::panel,     "cm7"},
      {Key::session,   "cs"},
      {Key::size,      "cm1"},
      {Key::status,    "cd1"},
      {Key::step,      "cm9"},
      {Key::tag,       "cd"},
      {Key::timestamp, "cm5"},
      {Key::value,     "cd2"},
      {Key::width,     "cm3"},
    };


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

      request.post_field(keymap.at(Key::tag), metric.second.at(Key::tag));

      for (Field f: metric.second)
      {
        if (f.first != Key::tag)
          request.post_field(keymap.at(f.first), f.second);
      };

      this->_last_sent.Current();
      auto delta = (this->_last_sent - metric.first).nanoseconds / 1000000UL;
      request.post_field("qt", std::to_string(delta));

      request.fire();
    }

    std::string
    Google::_format_event_name(std::string const& name)
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
