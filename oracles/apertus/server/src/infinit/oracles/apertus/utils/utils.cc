#include <json_spirit/reader.h>
#include <json_spirit/writer.h>

#include <infinit/oracles/apertus/utils/utils.hh>

namespace infinit
{
  namespace oracles
  {
    namespace apertus
    {
      namespace server
      {
        std::map<std::string, json_spirit::Value>
        read_json(reactor::network::TCPSocket& socket)
        {
          elle::IOStreamClear clearer(socket);
          json_spirit::Value value;
          if (!json_spirit::read(socket, value))
            throw;
          if (value.type() != json_spirit::Value::OBJECT_TYPE)
            throw;
          return value.getObject();
        }

        void
        write_json(reactor::network::TCPSocket& socket,
                   json_spirit::Value const& value)
        {
          elle::IOStreamClear clearer(socket);
          json_spirit::write(value, socket);
          socket << "\n";
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
