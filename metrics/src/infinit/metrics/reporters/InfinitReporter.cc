#include <functional>

#include <infinit/metrics/reporters/InfinitReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

//ELLE_LOG_COMPONENT("infinit.metrics.InfinitReporter");

namespace infinit
{
  namespace metrics
  {
    /*-------------.
    | Construction |
    `-------------*/

    InfinitReporter::InfinitReporter():
      Super("infinit reporter"),
      _base_url(),
      _port()
    {
      this->_base_url =
        elle::os::getenv("INFINIT_METRICS_HOST",
                         "v3.metrics.api.production.infinit.io");
      this->_port = boost::lexical_cast<int>(
        elle::os::getenv("INFINIT_METRICS_PORT", "80"));
    }

    /*-----.
    | Send |
    `-----*/

    void
    InfinitReporter::_post(std::string const& destination,
                           elle::json::Object data)
    {
      auto url = elle::sprintf(
        "http://%s:%d/%s", this->_base_url, this->_port, destination);
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
