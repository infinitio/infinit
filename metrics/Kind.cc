#include "Kind.hh"

#include <ostream>

namespace metrics
{
  std::ostream&
  operator <<(std::ostream& out,
              Kind const k)
  {
    switch (k)
    {
      case Kind::all:
        return out << "all";
      case Kind::user:
        return out << "user";
      case Kind::network:
        return out << "network";
      case Kind::transaction:
        return out << "transaction";
    }
    return out << "Unknown metrics key";
  }
}
