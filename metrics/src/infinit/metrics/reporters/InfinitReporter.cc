#include <functional>

#include <infinit/metrics/reporters/InfinitReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

ELLE_LOG_COMPONENT("infinit.metrics.InfinitReporter");

namespace infinit
{
  namespace metrics
  {
    /*-------------.
    | Construction |
    `-------------*/

    InfinitReporter::InfinitReporter(std::string const& url,
                                     int port):
      Super("infinit reporter"),
      _base_url(url),
      _port(port)
    {}

    /*-----.
    | Send |
    `-----*/

    void
    InfinitReporter::_post(std::string const& destination,
                           elle::json::Object data)
    {
      auto url = elle::sprintf(
        "http://%s:%d/%s", this->_base_url, this->_port, destination);
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
