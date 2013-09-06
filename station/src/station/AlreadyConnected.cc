#include <station/AlreadyConnected.hh>

namespace station
{
  AlreadyConnected::AlreadyConnected():
    ConnectionFailure("already connected to this host")
  {}
}
