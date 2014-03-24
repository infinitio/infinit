#include <functional>

#include <infinit/metrics/reporters/KeenReporter.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <reactor/exception.hh>
#include <reactor/http/exceptions.hh>
#include <reactor/http/Request.hh>
#include <reactor/scheduler.hh>

// ELLE_LOG_COMPONENT("infinit.metrics.KeenReporter");

namespace infinit
{
  namespace metrics
  {
    /*-------------.
    | Construction |
    `-------------*/

    KeenReporter::KeenReporter(std::string const& project,
                               std::string const& key):
      Super("Keen.io reporter", reactor::http::StatusCode::Created),
      _base_url(elle::sprintf("https://api.keen.io/3.0/projects/%s/events/%%s?api_key=%s", project, key))
    {}

    /*-----.
    | Send |
    `-----*/

    std::string
    KeenReporter::_url(std::string const& destination) const
    {
      return elle::sprintf(this->_base_url, destination);
    }
  }
}
