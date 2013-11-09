#include <json_spirit/reader.h>
#include <json_spirit/writer.h>

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
        std::map<std::string, json_spirit::Value>
        read_json(reactor::network::TCPSocket& socket)
        {
          elle::IOStreamClear clearer(socket);
          json_spirit::Value value;
          if (!json_spirit::read(socket, value))
            throw ProtocolError(elle::sprintf("JSON error"));
          if (value.type() != json_spirit::Value::OBJECT_TYPE)
            throw ProtocolError("json is not an object");
          return value.getObject();
        }

        void
        write_json(reactor::network::TCPSocket& socket,
                   json_spirit::Value const& value)
        {
          elle::IOStreamClear clearer(socket);
          json_spirit::write(value, socket);
          socket.flush();
        }

        std::string
        pretty_print_json(json_spirit::Value const& value)
        {
          return json_spirit::write(value, json_spirit::pretty_print);
        }
      }
    }
  }
}
