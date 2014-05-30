#ifndef SURFACE_GAP_STATE_ERROR_HH
# define SURFACE_GAP_STATE_ERROR_HH

# include <elle/Error.hh>

namespace infinit
{
  namespace state
  {
    class Error
      : public elle::Error
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef Error Self;
      typedef elle::Error Super;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      inline Error(std::string const& message);
    };

    class LoginError
      : public Error
    {
    public:
      inline LoginError(std::string const& message);
    };

    class CredentialError
      : public LoginError
    {
    public:
      inline CredentialError();
    };

    class UnconfirmedEmailError
      : public LoginError
    {
    public:
      inline UnconfirmedEmailError();
    };

    class AlreadyLoggedIn
      : public LoginError
    {
    public:
      inline AlreadyLoggedIn();
    };
  }
}

# include <surface/gap/Error.hxx>

#endif