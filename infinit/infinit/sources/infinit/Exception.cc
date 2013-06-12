#include <infinit/Exception.hh>

namespace infinit
{
  Exception::Exception(elle::String const& message):
    elle::Exception(message)
  {}
}
