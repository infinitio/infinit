#include <infinit/oracles/trophonius/server/exceptions.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        ProtocolError::ProtocolError(std::string const& message):
          elle::Exception(message)
        {}

        AuthenticationError::AuthenticationError(std::string const& message):
          elle::Exception(message)
        {}
      }
    }
  }
}
