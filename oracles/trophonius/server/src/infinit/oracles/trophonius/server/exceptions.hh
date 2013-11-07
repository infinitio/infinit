#ifndef INFINIT_ORACLES_TROPHONIUS_SERVER_EXCEPTIONS_HH
# define INFINIT_ORACLES_TROPHONIUS_SERVER_EXCEPTIONS_HH

# include <elle/Exception.hh>

namespace infinit
{
  namespace oracles
  {
    namespace trophonius
    {
      namespace server
      {
        class ProtocolError:
          public elle::Exception
        {
        public:
          ProtocolError(std::string const& message);
        };

        class AuthenticationError:
          public elle::Exception
        {
        public:
          AuthenticationError(std::string const& message);
        };
      }
    }
  }
}

#endif
