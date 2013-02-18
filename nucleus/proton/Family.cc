#include <iostream>
#include <stdexcept>

#include <elle/printf.hh>

#include <nucleus/proton/Family.hh>
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
                Family const family)
    {
      switch (family)
        {
        case Family::content_hash_block:
          {
            stream << "content hash block";
            break;
          }
        case Family::public_key_block:
          {
            stream << "public key block";
            break;
          }
        case Family::owner_key_block:
          {
            stream << "owner key block";
            break;
          }
        case Family::imprint_block:
          {
            stream << "imprint block";
            break;
          }
        default:
          {
            throw Exception(elle::sprintf("unknown family: '%s'",
                                          static_cast<int>(family)));
          }
        }

      return (stream);
    }
  }
}
