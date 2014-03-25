#include <functional>

#include <infinit/metrics/reporters/InfinitReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

// ELLE_LOG_COMPONENT("infinit.metrics.InfinitReporter");

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

    std::string
    InfinitReporter::_url(std::string const& destination) const
    {
      return elle::sprintf(
        "http://%s:%d/%s", this->_base_url, this->_port, destination);
    }
  }
}
