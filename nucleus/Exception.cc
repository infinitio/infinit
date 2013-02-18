#include <nucleus/Exception.hh>

namespace nucleus
{
  Exception::Exception(elle::String const& message):
    elle::Exception(message)
  {}
}
