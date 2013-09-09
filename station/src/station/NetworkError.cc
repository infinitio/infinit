#include <elle/printf.hh>

#include <station/NetworkError.hh>

namespace station
{
  NetworkError::NetworkError(reactor::network::Exception const& e):
    ConnectionFailure(elle::sprintf("network error: %s", e.what()))
  {}
}
