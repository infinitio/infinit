#include <iostream>

#include <elle/printf.hh>

#include <nucleus/proton/Strategy.hh>
#include <nucleus/Exception.hh>

namespace nucleus
{
  namespace proton
  {
    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Strategy const strategy)
    {
      switch (strategy)
        {
        case Strategy::none:
          {
            stream << "none";
            break;
          }
        case Strategy::value:
          {
            stream << "value";
            break;
          }
        case Strategy::block:
          {
            stream << "block";
            break;
          }
        case Strategy::tree:
          {
            stream << "tree";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown strategy: '%s'",
                                          static_cast<int>(strategy)));
          }
        }

      return (stream);
    }
  }
}
