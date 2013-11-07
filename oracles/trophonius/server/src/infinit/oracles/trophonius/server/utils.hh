#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_UTILS_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_UTILS_HH

# include <jsoncpp/json/json.h>

# include <reactor/network/tcp-socket.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        Json::Value
        read_json(reactor::network::TCPSocket& socket);
      }
    }
  }
}

#endif
