#ifndef STATION_INVALIDPASSPORT_HH
# define STATION_INVALIDPASSPORT_HH

# include <station/ConnectionFailure.hh>

namespace station
{
  class InvalidPassport:
    public ConnectionFailure
  {
  public:
    InvalidPassport();
  };
}

#endif
