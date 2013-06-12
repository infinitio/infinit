#ifndef INFINIT_IDENTITY_HXX
# define INFINIT_IDENTITY_HXX

# include <cryptography/SecretKey.hh>

namespace infinit
{
  /*--------------.
  | Cnonstruction |
  `--------------*/

  template <typename T>
  Identity::Identity(elle::String identifier,
                     elle::String name,
                     cryptography::KeyPair const& pair,
                     elle::String const& passphrase,
                     T const& authority):
    Identity(std::move(identifier),
             std::move(name),
             pair,
             cryptography::SecretKey(Identity::Constants::cipher_algorithm,
                                     passphrase),
             authority)
  {
  }

  template <typename T>
  Identity::Identity(elle::String identifier,
                     elle::String name,
                     cryptography::KeyPair const& pair,
                     cryptography::SecretKey const& key,
                     T const& authority):
    Identity(std::move(identifier),
             std::move(name),
             key.encrypt(pair),
             authority)
  {
  }

  template <typename T>
  Identity::Identity(elle::String identifier,
                     elle::String name,
                     cryptography::Code keypair,
                     T const& authority):
    Identity(std::move(identifier),
             std::move(name),
             std::move(keypair),
             authority.sign(identity::hash(identifier, name, keypair)))
  {
  }

  /*--------.
  | Methods |
  `--------*/

  template <typename T>
  elle::Boolean
  Identity::validate(T const& authority) const
  {
    return (authority.verify(this->_signature,
                             identity::hash(this->_identifier,
                                            this->_name,
                                            this->_keypair)));
  }
}

/*-----------.
| Serializer |
`-----------*/

# include <elle/serialize/Pointer.hh>
# include <elle/serialize/insert.hh>
# include <elle/serialize/extract.hh>

# include <cryptography/Code.hh>
# include <cryptography/Signature.hh>
# include <cryptography/KeyPair.hh>

# include <infinit/Exception.hh>

ELLE_SERIALIZE_STATIC_FORMAT(infinit::Identity, 1);

ELLE_SERIALIZE_SPLIT(infinit::Identity);

ELLE_SERIALIZE_SPLIT_SAVE(infinit::Identity,
                          archive,
                          value,
                          format)
{
  switch (format)
  {
    case 0:
    {
      // XXX remove the temporary variables when the serialization mechanism
      //     will be able to handle const rvalues.

      cryptography::Code const* _keypair = &value._keypair;
      archive << elle::serialize::pointer(_keypair);

      // Generate a key pair because the format 0 used to contain
      // the key pair in clear.
      //
      // So rather than keeping the actual key pair (which represents a
      // security breach), generate a useless key pair just for the
      // serialization mechanism to function.
      cryptography::KeyPair random =
        cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                        1024);
      cryptography::KeyPair const* _random(&random);
      archive << elle::serialize::alive_pointer(_random);

      archive << value._identifier;
      archive << value._name;

      cryptography::Signature const* _signature(&value._signature);
      archive << elle::serialize::alive_pointer(_signature);

      break;
    }
    case 1:
    {
      archive << value._identifier;
      archive << value._name;
      archive << value._keypair;
      archive << value._signature;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

ELLE_SERIALIZE_SPLIT_LOAD(infinit::Identity,
                          archive,
                          value,
                          format)
{
  switch (format)
  {
    case 0:
    {
      cryptography::Code* keypair_pointer = nullptr;
      archive >> elle::serialize::pointer(keypair_pointer);
      std::unique_ptr<cryptography::Code> keypair(keypair_pointer);

      // Re-serialize the coded keypair and deserialize it in the _keypair
      // attribute so as to transform a pointer into a value.
      enforce(keypair != nullptr,
              "unable to support identity with no coded keypair");
      elle::String keypair_archive;
      elle::serialize::to_string(keypair_archive) << *keypair;
      elle::serialize::from_string(keypair_archive) >> value._keypair;

      cryptography::KeyPair* forget_pointer = nullptr;
      archive >> elle::serialize::alive_pointer(forget_pointer);
      std::unique_ptr<cryptography::KeyPair> forget(forget_pointer);
      // Forget the pair as we should never have had access to
      // it from the archive anyway.
      (void)keypair;

      archive >> value._identifier;
      archive >> value._name;

      cryptography::Signature* signature_pointer = nullptr;
      archive >> elle::serialize::alive_pointer(signature_pointer);
      std::unique_ptr<cryptography::Signature> signature(signature_pointer);

      // Re-serialize the signature and deserialize it in the _signature
      // attribute so as to transform a pointer into a value.
      elle::String signature_archive;
      elle::serialize::to_string(signature_archive) << *signature;
      elle::serialize::from_string(signature_archive) >> value._signature;

      break;
    }
    case 1:
    {
      archive >> value._identifier;
      archive >> value._name;
      archive >> value._keypair;
      archive >> value._signature;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

#endif
