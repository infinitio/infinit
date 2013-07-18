#include "Service.hh"

#include <metrics/Reporter.hh>

#include <curly/curly.hh>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

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

  int
  Service::_curl_debug_callback(CURL* handle,
                               curl_infotype type,
                               char* what,
                               size_t what_size,
                               void* userptr)
  {
    ELLE_LOG_COMPONENT("metrics.curl");
    (void)handle;

    Service* client = reinterpret_cast<Service*>(userptr);

    std::map<curl_infotype, std::string> symbols = {
      {CURLINFO_TEXT, "*"},
      {CURLINFO_HEADER_IN, "<"},
      {CURLINFO_HEADER_OUT, ">"},
      {CURLINFO_DATA_IN, "<<"},
      {CURLINFO_DATA_OUT, ">>"},
    };
    // get rid of \n
    if (what[what_size - 1] == '\n')
    {
      what_size--;
      what[what_size] = '\0';
    }

    std::string msg{what, what + what_size};
    std::string sym;
    try
    {
      sym = symbols.at(type);
    }
    catch (std::out_of_range const&)
    {
      sym = "*";
    }

    if (type == CURLINFO_TEXT)
    {
      ELLE_DUMP("%s: %s %s", *client, sym, msg);
    }
    else if (type == CURLINFO_HEADER_OUT)
    {
      std::vector<std::string> v;

      boost::split(v, msg, boost::algorithm::is_any_of("\n"));
      ELLE_TRACE_SCOPE("%s: %s %s", *client, sym, v[0]);
      int i = 0;
      for (auto const&s : v)
      {
        if (i++ == 0)
          continue;
        ELLE_DEBUG("%s: %s %s", *client, sym, s);
      }
    }
    else if (type == CURLINFO_DATA_IN || type == CURLINFO_DATA_OUT)
    {
      ELLE_TRACE("%s: %s %s", *client, sym, msg);
    }
    else if (type == CURLINFO_HEADER_IN)
    {
      if (msg.find("HTTP") == 0) // starts with
        ELLE_TRACE("%s: %s %s", *client, sym, msg);
      else
        ELLE_DUMP("%s: %s %s", *client, sym, msg);
    }
    return 0;
  }
}
