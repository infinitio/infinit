#include "Service.hh"

#include <metrics/Reporter.hh>

namespace metrics
{
  Service::Service(std::string const& pkey,
                   common::metrics::Info const& info):
    _pkey{pkey},
    _info(info),
    _server{
      new elle::HTTPClient{_info.host, _info.port, Reporter::user_agent}}
  {}

  Service::~Service()
  {}
}
