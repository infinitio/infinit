#include <hole/Exception.hh>

namespace hole
{
  /*-------------.
  | Construction |
  `-------------*/

  Exception::Exception(elle::String const& message):
    elle::Exception(message)
  {}
}
