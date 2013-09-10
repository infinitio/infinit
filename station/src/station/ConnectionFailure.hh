#ifndef STATION_CONNECTIONFAILURE_HH
# define STATION_CONNECTIONFAILURE_HH

# include <elle/Exception.hh>

namespace station
{
  class ConnectionFailure:
    public elle::Exception
  {
  public:
    template <typename... Args>
    ConnectionFailure(Args&&... args):
      elle::Exception(std::forward<Args>(args)...)
    {}
  };
}

#endif
