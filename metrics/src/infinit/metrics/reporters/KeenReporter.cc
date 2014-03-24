#include <functional>

#include <infinit/metrics/reporters/KeenReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

//ELLE_LOG_COMPONENT("infinit.metrics.KeenReporter");

namespace infinit
{
  namespace metrics
  {
    /*-------------.
    | Construction |
    `-------------*/

    KeenReporter::KeenReporter():
      Super("Keen.io reporter"),
      _base_url("https://api.keen.io/3.0/projects/532c5a9c00111c0da2000023/events/%s?api_key=19562aa3aed59df3f0a0bb746975d4b61a1789b52b6ee42ffcdd88fbe9fec7bd6f8e6cf4256fee1a08a842edc8212b98b57d3c28b6df94fd1520834390d0796ad2efbf59ee1fca268bdc4c6d03fa438102ae22c7c6e318d98fbe07becfb83ec65b2e844c57bb3db2da1d36903c4ef791")
    {}

    /*-----.
    | Send |
    `-----*/

    void
    KeenReporter::_post(std::string const& destination,
                           elle::json::Object data)
    {
      auto url = elle::sprintf(this->_base_url, destination);
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
