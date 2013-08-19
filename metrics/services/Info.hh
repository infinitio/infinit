#ifndef METRICS_SERVICE_INFO_HH
# define METRICS_SERVICE_INFO_HH

namespace metrics
{
  namespace services
  {
    struct Info
    {
      std::string const pretty_name;
      std::string const host;
      uint16_t const port;
      std::string const id_path;
      std::string const tracking_id;
    };
  }
}

#endif
