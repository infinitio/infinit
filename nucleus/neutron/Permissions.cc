#include <iostream>
#include <stdexcept>

#include <elle/printf.hh>

#include <nucleus/neutron/Permissions.hh>
#include <nucleus/Exception.hh>

namespace nucleus
{
  namespace neutron
  {
    /*-------.
    | Values |
    `-------*/

    namespace permissions
    {
      Permissions const none = 0;
      Permissions const read = (1 << 0);
      Permissions const write = read | (1 << 1);
    }

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Permissions const permissions)
    {
      switch (permissions)
        {
        case permissions::none:
          {
            stream << "none";
            break;
          }
        case permissions::read:
          {
            stream << "read";
            break;
          }
        case permissions::write:
          {
            stream << "write";
            break;
          }
        default:
          throw Exception(elle::sprintf("unknown permissions '%s'",
                                        static_cast<int>(permissions)));
        }

      return (stream);
    }
  }
}
