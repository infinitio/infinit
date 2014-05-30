#include <surface/gap/Error.hh>

namespace infinit
{
  namespace state
  {
    Error::Error(std::string const& message)
      : Super(message)
    {}

    LoginError::LoginError(std::string const& message)
      : Error::Error(message)
    {}

    CredentialError::CredentialError()
      : LoginError::LoginError("Email/password are incorrect.")
    {}

    UnconfirmedEmailError::UnconfirmedEmailError()
      : LoginError::LoginError("Email not confirmed.")
    {}

    AlreadyLoggedIn::AlreadyLoggedIn()
      : LoginError::LoginError("Already logged in.")
    {}
  }
}
