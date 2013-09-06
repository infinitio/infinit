#include <station/InvalidPassport.hh>

namespace station
{
  InvalidPassport::InvalidPassport():
    ConnectionFailure("remote host passport is invalid")
  {}
}
