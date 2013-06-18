#include "google.hh"

#include <elle/format/hexadecimal.hh>
#include <elle/os/getenv.hh>
#include <elle/memory.hh>
#include <elle/os/path.hh>

#include <cryptography/Digest.hh>
#include <cryptography/Plain.hh>
#include <cryptography/oneway.hh>

#include <common/common.hh>


ELLE_LOG_COMPONENT("elle.metrics.google.Service");

namespace elle
{
  namespace metrics
  {
    namespace google
    {
      static std::string pretty_name = "Google";
      static const std::map<elle::metrics::Key, std::string> keymap
      {
        {elle::metrics::Key::author,    "cm8"},
        {elle::metrics::Key::count,     "cm2"},
        {elle::metrics::Key::height,    "cm4"},
        {elle::metrics::Key::input,     "cm6"},
        {elle::metrics::Key::panel,     "cm7"},
        {elle::metrics::Key::session,   "cs"},
        {elle::metrics::Key::size,      "cm1"},
        {elle::metrics::Key::status,    "cd1"},
        {elle::metrics::Key::step,      "cm9"},
        {elle::metrics::Key::tag,       "cd"},
        {elle::metrics::Key::timestamp, "cm5"},
        {elle::metrics::Key::value,     "cd2"},
        {elle::metrics::Key::width,     "cm3"},
      };


      //- Service --------------------------------------------------------------
      Service::Service():
        elle::metrics::Reporter::Service{
          common::metrics::google_info().host,
          common::metrics::google_info().port,
          retrieve_id(common::metrics::google_info().id_path),
          pretty_name}
        , _id_file_path{common::metrics::google_info().id_path}
      {}

      void
      Service::_send(Reporter::TimeMetricPair const& metric)
      {
        ELLE_TRACE("sending metric");

        // XXX copy constructor with should great idea, cause request base here
        // is always the same, so it could be static.
        elle::Request request = this->_server->request("POST", "/collect");
        request
          .content_type("application/x-www-form-urlencoded")
          .user_agent(elle::metrics::Reporter::user_agent)
          .post_field("dh", "infinit.io")      // Test.
          .post_field("av", elle::metrics::Reporter::version) // Type of interraction.
          .post_field("an", "Infinit")         // Application name.
          .post_field("t", "appview")          // Type of interraction.
          .post_field("cid", this->_user_id)   // Anonymous user.
          .post_field("tid", common::metrics::google_info().tracking_id)
          .post_field("v", "1");               // Api version.

        typedef Reporter::Metric::value_type Field;

        request.post_field(keymap.at(Key::tag), metric.second.at(Key::tag));

        for (Field f: metric.second)
        {
          if (f.first != Key::tag)
            request.post_field(keymap.at(f.first), f.second);
        };

        _last_sent.Current();
        request.post_field("qt",
                           std::to_string((_last_sent - metric.first).nanoseconds / 1000000));

        request.fire();
      }

      void
      Service::update_user(std::string const& id)
      {
        elle::Buffer id_buffer(reinterpret_cast<elle::Byte const*>(id.data()), id.length());

        infinit::cryptography::Digest digest = infinit::cryptography::oneway::hash(
          infinit::cryptography::Plain(
            elle::WeakBuffer{id_buffer}),
          infinit::cryptography::oneway::Algorithm::sha256);

        std::string hashed_id =
          elle::format::hexadecimal::encode(digest.buffer());

        ELLE_DEBUG("hashed id: %s", hashed_id);

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

        ELLE_DEBUG("hashed id: %s", hashed_id);

        std::ofstream id_file(this->_id_file_path);

        if (id_file.good())
        {
          id_file << hashed_id;
          id_file.close();
        }

        this->_user_id = hashed_id;
      }

      //- Helper ---------------------------------------------------------------
      void
      register_service(Reporter& reporter)
      {
        reporter.add_service(elle::make_unique<Service>());
      }

      std::string
      retrieve_id(std::string const& path)
      {
        std::string id = "66666666-6666-6666-6666-66666666";

        if (elle::os::path::exists(path))
        {
          std::ifstream id_file(path);

          if (!id_file.good())
            return id;

          std::getline(id_file, id);

          id_file.close();
          // should be checked by regex but std regex suxx.
        }

        return id;
      }
    } // End of google
  } // End of metrics
} // End of elle.
