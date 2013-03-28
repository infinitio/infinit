#include <etoile/Exception.hh>

namespace etoile
{
  /*-------------.
  | Construction |
  `-------------*/

  Exception::Exception(elle::String const& message):
    elle::Exception(message)
  {}
}
