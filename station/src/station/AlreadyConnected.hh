#ifndef STATION_ALREADYCONNECTED_HH
# define STATION_ALREADYCONNECTED_HH

# include <station/ConnectionFailure.hh>

namespace station
{
  class AlreadyConnected:
    public ConnectionFailure
  {
  public:
    AlreadyConnected();
  };
}

#endif
