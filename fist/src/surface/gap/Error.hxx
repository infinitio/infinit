namespace infinit
{
  namespace state
  {
    Error::Error(std::string const& message)
      : Super(message)
    {}

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

    TransactionFinalized::TransactionFinalized()
      : Error("transaction is already finalized")
    {}
  }
}
