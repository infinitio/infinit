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
    switch (k)
    {
    case Key::attempt:
      return out << "attempt";
    case Key::author:
      return out << "author";
    case Key::count:
      return out << "count";
    case Key::duration:
      return out << "duration";
    case Key::height:
      return out << "height";
    case Key::input:
      return out << "input";
    case Key::network:
      return out << "network";
    case Key::panel:
      return out << "panel";
    case Key::session:
      return out << "session";
    case Key::size:
      return out << "size";
    case Key::status:
      return out << "status";
    case Key::step:
      return out << "step";
    case Key::tag:
      return out << "tag";
    case Key::timestamp:
      return out << "timestamp";
    case Key::value:
      return out << "value";
    case Key::width:
      return out << "width";
    case Key::sender_online:
      return out << "sender_online";
    case Key::recipient_online:
      return out << "recipient_online";
    }
    return out << "Unknown metrics key";
  }
}
