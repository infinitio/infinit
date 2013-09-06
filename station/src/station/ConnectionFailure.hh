#ifndef STATION_CONNECTIONFAILURE_HH
# define STATION_CONNECTIONFAILURE_HH

# include <elle/Exception.hh>

class ConnectionFailure:
  public elle::Exception
{
public:
  using elle::Exception::Exception;
};

#endif
