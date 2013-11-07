#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>

#include <infinit/oracles/trophonius/server/exceptions.hh>
#include <infinit/oracles/trophonius/server/utils.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        Json::Value
        read_json(reactor::network::TCPSocket& socket)
        {
          auto buffer = socket.read_until("\n");
          Json::Value root;
          Json::Reader reader;

          bool res = reader.parse(
            reinterpret_cast<char*>(buffer.contents()),
            reinterpret_cast<char*>(buffer.contents()) + buffer.size(),
            root, false);
          if (!res)
          {
            auto error = reader.getFormatedErrorMessages();
            throw ProtocolError(elle::sprintf("JSON error: %s", error));
          }
          return root;
        }
      }
    }
  }
}
