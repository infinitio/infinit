#include "kissmetrics.hh"

#include <elle/format/hexadecimal.hh>
#include <elle/memory.hh>
#include <elle/os/path.hh>

#include <cryptography/Digest.hh>
#include <cryptography/Plain.hh>
#include <cryptography/oneway.hh>

#include <common/common.hh>


ELLE_LOG_COMPONENT("elle.metrics.kissmetrics.Service");

namespace elle
{
  namespace metrics
  {
    namespace kissmetrics
    {
      static std::string pretty_name = "Kissmetrics";
      static const std::map<elle::metrics::Key, std::string> keymap
      {
        {elle::metrics::Key::author,    "author"},
        {elle::metrics::Key::count,     "count"},
        {elle::metrics::Key::height,    "height"},
        {elle::metrics::Key::input,     "input"},
        {elle::metrics::Key::panel,     "panel"},
        {elle::metrics::Key::session,   "session"},
        {elle::metrics::Key::size,      "size"},
        {elle::metrics::Key::status,    "status"},
        {elle::metrics::Key::step,      "step"},
        {elle::metrics::Key::tag,       "_n"},
        {elle::metrics::Key::timestamp, "timestamp"},
        {elle::metrics::Key::value,     "value"},
        {elle::metrics::Key::width,     "witdh"},
      };

      //- Service --------------------------------------------------------------
      Service::Service():
        elle::metrics::Reporter::Service{
          common::metrics::km_info().host,
          common::metrics::km_info().port,
          retrieve_id(common::metrics::km_info().id_path),
          pretty_name}
        , _id_file_path{common::metrics::km_info().id_path}
      {}

      void
      Service::_send(Reporter::TimeMetricPair const& metric)
      {
        ELLE_TRACE("sending metric");

        // XXX copy constructor with should great idea, cause request base here
        // is always the same, so it could be static.
        elle::Request request = this->_server->request("GET", "/e");
        request
          .user_agent(elle::metrics::Reporter::user_agent)
          .parameter("version", elle::metrics::Reporter::version)
          .parameter("app_name", "Infinit")
          .parameter("_p", this->_user_id)
          .parameter("_k", common::metrics::km_info().tracking_id)
          .parameter("version", "1");

        typedef Reporter::Metric::value_type Field;

        request.parameter(keymap.at(Key::tag), metric.second.at(Key::tag));

        for (Field f: metric.second)
        {
          if (f.first != Key::tag)
            request.parameter(keymap.at(f.first), f.second);
        };

        request.fire();
      }

      void
      Service::update_user(std::string const& id)
      {
        std::ofstream id_file(this->_id_file_path);

        if (id_file.good())
        {
          id_file << id;
          id_file.close();
        }

        this->_user_id = id;
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
        std::string id = "anonymous@infinit.io";

        if (elle::os::path::exists(path))
        {
          std::ifstream id_file(path);

          if(!id_file.good())
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
