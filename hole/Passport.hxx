#ifndef HOLE_PASSPORT_HXX
# define HOLE_PASSPORT_HXX

# include <hole/Exception.hh>

namespace hole
{
  /*-------------.
  | Construction |
  `-------------*/

  template <typename O,
            typename A>
  Passport::Passport(cryptography::PublicKey authority_K,
                     cryptography::PublicKey owner_K,
                     elle::String description,
                     O const& owner,
                     A const& authority,
                     Identifier identifier):
    Passport(std::move(authority_K),
             std::move(owner_K),
             std::move(description),
             owner.sign(passport::hash(description)),
             authority,
             std::move(identifier))
  {}

  template <typename A>
  Passport::Passport(cryptography::PublicKey authority_K,
                     cryptography::PublicKey owner_K,
                     elle::String description,
                     cryptography::Signature owner_signature,
                     A const& authority,
                     Identifier identifier):
    Passport(std::move(authority_K),
             std::move(owner_K),
             std::move(description),
             std::move(owner_signature),
             authority.sign(passport::hash(identifier,
                                           owner_K,
                                           owner_signature)),
             std::move(identifier))
  {}

  /*--------.
  | Methods |
  `--------*/

  template <typename T>
  elle::Boolean
  Passport::validate(T const& authority) const
  {
    ELLE_LOG_COMPONENT("hole.Passport");
    ELLE_TRACE_METHOD(authority);

    // XXX improve this verification mechanism so as to validate the
    //     authority as well through a chain of certificate.

    ELLE_DEBUG("format: %s",
               this->hole::Passport::DynamicFormat::version());

    switch (this->hole::Passport::DynamicFormat::version())
    {
      case 0:
      {
        return (authority.verify(this->_authority_signature,
                                 passport::hash_0(this->_identifier.value(),
                                                  this->_owner_K)));
      }
      case 1:
      {
        if (authority.verify(this->_authority_signature,
                             passport::hash(this->_identifier,
                                            this->_owner_K,
                                            this->_owner_signature)) == false)
          return (false);

        if (this->_owner_K.verify(this->_owner_signature,
                                  passport::hash(this->_description)) == false)
          return (false);

        return (true);
      }
      default:
        throw ::hole::Exception(
          elle::sprintf(
            "unknown format '%s'",
            this->hole::Passport::DynamicFormat::version()));
    }

    elle::unreachable();
  }
}

/*-----------.
| Serializer |
`-----------*/

# include <elle/serialize/insert.hh>
# include <elle/serialize/extract.hh>

# include <cryptography/KeyPair.hh>

ELLE_SERIALIZE_STATIC_FORMAT(hole::Passport, 1);

ELLE_SERIALIZE_SPLIT(hole::Passport);

ELLE_SERIALIZE_SPLIT_SAVE(hole::Passport,
                          archive,
                          value,
                          format)
{
  switch (format)
  {
    case 0:
    {
      archive << value._identifier.value();
      archive << value._description;
      archive << value._owner_K;
      archive << value._authority_signature;

      break;
    }
    case 1:
    {
      archive << value._authority_K;
      archive << value._identifier;
      archive << value._owner_K;
      archive << value._description;
      archive << value._owner_signature;
      archive << value._authority_signature;

      break;
    }
    default:
      throw ::hole::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

ELLE_SERIALIZE_SPLIT_LOAD(hole::Passport,
                          archive,
                          value,
                          format)
{
  switch (format)
  {
    case 0:
    {
      // Extract the identifier string-based value, construct an identifier and
      // serialize it so as to be able to extract it in the existing
      // identifier attribute.
      elle::String identifier_string;
      archive >> identifier_string;
      infinit::Identifier identifier(identifier_string);
      elle::String identifier_archive;
      elle::serialize::to_string(identifier_archive) << identifier;
      elle::serialize::from_string(identifier_archive) >> value._identifier;

      archive >> value._description;
      archive >> value._owner_K;
      archive >> value._authority_signature;

      // Just to make sure the passport's attributes are all valid, generate
      // a key pair and set the remaining attributes.
      cryptography::KeyPair keypair =
        cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                        1024);
      elle::String authority_K_string;
      elle::serialize::to_string(authority_K_string) << keypair.K();
      elle::serialize::from_string(authority_K_string) >> value._authority_K;

      elle::String something = "something";
      cryptography::Signature owner_signature = keypair.k().sign(something);
      elle::String owner_signature_string;
      elle::serialize::to_string(owner_signature_string) <<
        owner_signature;
      elle::serialize::from_string(owner_signature_string) >>
        value._owner_signature;

      break;
    }
    case 1:
    {
      archive >> value._authority_K;
      archive >> value._identifier;
      archive >> value._owner_K;
      archive >> value._description;
      archive >> value._owner_signature;
      archive >> value._authority_signature;

      break;
    }
    default:
      throw ::hole::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

#endif
