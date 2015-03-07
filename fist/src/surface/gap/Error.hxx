#include <elle/printf.hh>

namespace infinit
{
  namespace state
  {
    /*-----------.
    | Base Error |
    `-----------*/
    Error::Error(std::string const& message)
      : Super(message)
    {}

    /*------------.
    | User Errors |
    `------------*/
    UserError::UserError(std::string const& message)
      : Error(message)
    {}

    UserNotFoundError::UserNotFoundError(std::string const& id_or_email)
      : UserError(elle::sprintf("user (%s) not found", id_or_email))
    {}

    /*-------------.
    | Login Errors |
    `-------------*/
    LoginError::LoginError(std::string const& message)
      : Error(message)
    {}

    CredentialError::CredentialError()
      : LoginError("email/password are incorrect")
    {}

    UnconfirmedEmailError::UnconfirmedEmailError()
      : LoginError("email not confirmed")
    {}

    AlreadyLoggedIn::AlreadyLoggedIn()
      : LoginError("already logged in")
    {}

    VersionRejected::VersionRejected()
      : LoginError("version rejected")
    {}

    /*-----------------.
    | Self User Errors |
    `-----------------*/
    SelfUserError::SelfUserError(std::string const& message)
      : Error(message)
    {}

    EmailAlreadyRegistered::EmailAlreadyRegistered()
      : SelfUserError("email already registered")
    {}

    EmailNotValid::EmailNotValid()
      : SelfUserError("email not valid")
    {}

    FullnameNotValid::FullnameNotValid()
      : SelfUserError("fullname not valid")
    {}

    HandleAlreadyRegistered::HandleAlreadyRegistered()
      : SelfUserError("handle already registered")
    {}

    PasswordNotValid::PasswordNotValid()
      : SelfUserError("password not valid")
    {}

    /*-------------------.
    | Transaction Errors |
    `-------------------*/
    TransactionFinalized::TransactionFinalized()
      : Error("transaction is already finalized")
    {}

    /*--------------.
    | Merge account |
    `--------------*/
    InvalidGhostCode::InvalidGhostCode()
      : Error("code is invalid")
    {}
  }
}
