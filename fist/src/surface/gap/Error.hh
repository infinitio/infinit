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

    /*------------.
    | User Errors |
    `------------*/
    class UserError
      : public Error
    {
    public:
      inline UserError(std::string const& message);
    };

    class UserNotFoundError
      : public UserError
    {
    public:
      inline UserNotFoundError(std::string const& id_or_email);
    };

    /*-------------.
    | Login Errors |
    `-------------*/
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

    class VersionRejected
      : public LoginError
    {
    public:
      inline VersionRejected();
    };

    /*-----------------.
    | Self User Errors |
    `-----------------*/
    class SelfUserError
      : public Error
    {
    public:
      inline SelfUserError(std::string const& message);
    };

    class EmailAlreadyRegistered
      : public SelfUserError
    {
    public:
      inline EmailAlreadyRegistered();
    };

    class EmailNotValid
      : public SelfUserError
    {
    public:
      inline EmailNotValid();
    };

    class FullnameNotValid
      : public SelfUserError
    {
    public:
      inline FullnameNotValid();
    };

    class HandleAlreadyRegistered
      : public SelfUserError
    {
    public:
      inline HandleAlreadyRegistered();
    };

    class PasswordNotValid
      : public SelfUserError
    {
    public:
      inline PasswordNotValid();
    };

    /*-------------------.
    | Transaction Errors |
    `-------------------*/
    class TransactionFinalized
      : public Error
    {
    public:
      inline TransactionFinalized();
    };
  }
}

# include <surface/gap/Error.hxx>

#endif
