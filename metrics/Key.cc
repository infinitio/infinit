#include "Key.hh"

#include <elle/assert.hh>

#include <ostream>
#include <unordered_map>

namespace metrics
{
  std::ostream&
  operator <<(std::ostream& out,
              Key const k)
  {
    static std::unordered_map<Key, std::string> const map{
      {Key::attempt,    "attempt"},
      {Key::author,     "author"},
      {Key::count,      "count"},
      {Key::duration,   "duration"},
      {Key::height,     "height"},
      {Key::input,      "input"},
      {Key::network,    "network"},
      {Key::panel,      "panel"},
      {Key::session,    "session"},
      {Key::size,       "size"},
      {Key::status,     "status"},
      {Key::step,       "step"},
      {Key::tag,        "tag"},
      {Key::timestamp,  "timestamp"},
      {Key::value,      "value"},
      {Key::width,      "width"},
    };
    ELLE_ASSERT(map.find(k) != map.end());
    return out << map.at(k);
  }
}
