#ifndef STATION_NETWORKERROR_HH
# define STATION_NETWORKERROR_HH

# include <reactor/network/exception.hh>

# include <station/ConnectionFailure.hh>

namespace station
{
  class NetworkError:
    public ConnectionFailure
  {
  public:
    NetworkError(reactor::network::Exception const& e);
  };
}

#endif
