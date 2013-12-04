#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_UTILS_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_UTILS_HH

#include <json_spirit/value.h>

# include <reactor/network/tcp-socket.hh>

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      namespace server
      {
        std::map<std::string, json_spirit::Value>
        read_json(reactor::network::TCPSocket& socket);

        void
        write_json(reactor::network::TCPSocket& socket,
                   json_spirit::Value const& value);

        std::string
        pretty_print_json(json_spirit::Value const& value);
      }
    }
  }
}

#endif
