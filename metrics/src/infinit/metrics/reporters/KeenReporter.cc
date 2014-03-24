#include <functional>

#include <infinit/metrics/reporters/KeenReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

ELLE_LOG_COMPONENT("infinit.metrics.KeenReporter");

namespace infinit
{
  namespace metrics
  {
    /*-------------.
    | Construction |
    `-------------*/

    KeenReporter::KeenReporter(std::string const& project,
                               std::string const& key):
      Super("Keen.io reporter"),
      _base_url(elle::sprintf("https://api.keen.io/3.0/projects/%s/events/%%s?api_key=%s", project, key))
    {}

    /*-----.
    | Send |
    `-----*/

    void
    KeenReporter::_post(std::string const& destination,
                           elle::json::Object data)
    {
      auto url = elle::sprintf(this->_base_url, destination);
      ELLE_TRACE_SCOPE("%s: send event to %s", *this, url);
      ELLE_DUMP("%s: data: %s", *this, data);
      reactor::http::Request::Configuration cfg(
        10_sec, reactor::http::Version::v11);
      cfg.header_add("User-Agent", Reporter::user_agent());
      reactor::http::Request r(
        url, reactor::http::Method::POST, "application/json", cfg);
      elle::json::write(r, data);
      reactor::wait(r);
    }
  }
}
