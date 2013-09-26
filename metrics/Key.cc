#include "Key.hh"

#include <ostream>

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
    case Key::connection_method:
      return out << "connection_method";
    case Key::duration:
      return out << "duration";
    case Key::file_count:
      return out << "file_count";
    case Key::file_size:
      return out << "file_size";
    case Key::height:
      return out << "height";
    case Key::how_ended:
      return out << "how_ended";
    case Key::input:
      return out << "input";
    case Key::method:
      return out << "method";
    case Key::metric_from:
      return out << "metric_from";
    case Key::network:
      return out << "network";
    case Key::panel:
      return out << "panel";
    case Key::recipient:
      return out << "recipient";
    case Key::sender:
      return out << "sender";
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
    case Key::transaction_id:
      return out << "transaction_id";
    case Key::value:
      return out << "value";
    case Key::width:
      return out << "width";
    case Key::sender_online:
      return out << "sender_online";
    case Key::recipient_online:
      return out << "recipient_online";
    case Key::source:
      return out << "source";
    case Key::who_connected:
      return out << "who_connected";
    case Key::who_ended:
      return out << "who_ended";
    }
    return out << "Unknown metrics key";
  }
}
