#include <elle/finally.hh>

#include <metrics/Reporter.hh>
#include <metrics/Service.hh>

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

  void
  Service::send(TimeMetricPair metric)
  {
    elle::Finally save_metric{
      [this, metric] {
        this->_queue.emplace_back(std::move(metric));
      }
    };
    this->_flush();
    this->_send(std::move(metric));
    // Everything went fine, no need to enqueue the metric.
    save_metric.abort();
  }

  void
  Service::_flush()
  {
    while (this->_queue.size() > 0)
    {
      this->_send(this->_queue.front());
      // Metric sent, we can remove it from the queue
      this->_queue.pop_front();
    }
  }
}
