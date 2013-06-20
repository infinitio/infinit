#ifndef INFINIT_IDENTITY_HXX
# define INFINIT_IDENTITY_HXX

# include <cryptography/SecretKey.hh>

# include <infinit/Exception.hh>

namespace infinit
{
  /*--------------.
  | Cnonstruction |
  `--------------*/

  template <typename T>
  Identity::Identity(cryptography::PublicKey issuer_K,
                     cryptography::PublicKey subject_K,
                     elle::String description,
                     cryptography::PrivateKey const& subject_k,
                     elle::String const& passphrase,
                     T const& authority,
                     Identifier identifier):
    Identity(std::move(issuer_K),
             std::move(subject_K),
             std::move(description),
             subject_k,
             cryptography::SecretKey(Identity::Constants::cipher_algorithm,
                                     passphrase),
             authority,
             std::move(identifier))
  {
  }

  template <typename T>
  Identity::Identity(cryptography::PublicKey issuer_K,
                     cryptography::PublicKey subject_K,
                     elle::String description,
                     cryptography::PrivateKey const& subject_k,
                     cryptography::SecretKey const& key,
                     T const& authority,
                     Identifier identifier):
    Identity(std::move(issuer_K),
             std::move(subject_K),
             std::move(description),
             key.encrypt(subject_k),
             authority,
             std::move(identifier))
  {
  }

  template <typename T>
  Identity::Identity(cryptography::PublicKey issuer_K,
                     cryptography::PublicKey subject_K,
                     elle::String description,
                     cryptography::Code subject_k,
                     T const& authority,
                     Identifier identifier):
    Identity(std::move(issuer_K),
             std::move(subject_K),
             std::move(description),
             std::move(subject_k),
             authority.sign(identity::hash(identifier,
                                           issuer_K,
                                           subject_K,
                                           description,
                                           subject_k)),
             std::move(identifier))
  {
  }

  /*--------.
  | Methods |
  `--------*/

  template <typename T>
  elle::Boolean
  Identity::validate(T const& authority) const
  {
    ELLE_LOG_COMPONENT("infinit.Identity");
    ELLE_TRACE_METHOD(authority);

    // XXX improve this verification mechanism so as to validate the
    //     authority as well through a chain of certificate.

    ELLE_DEBUG("format: %s",
               this->infinit::Identity::DynamicFormat::version());

    switch (this->infinit::Identity::DynamicFormat::version())
    {
      case 0:
      {
        ELLE_ASSERT_NEQ(this->_subject_keypair_0, nullptr);

        return (authority.verify(this->_signature,
                                 identity::hash_0(this->_identifier.value(),
                                                  this->_description,
                                                  *this->_subject_keypair_0)));
      }
      case 1:
      {
        return (authority.verify(this->_signature,
                                 identity::hash(this->_identifier,
                                                this->_issuer_K,
                                                this->_subject_K,
                                                this->_description,
                                                this->_subject_k)));
      }
      default:
        throw ::infinit::Exception(
          elle::sprintf(
            "unknown format '%s'",
            this->infinit::Identity::DynamicFormat::version()));
    }

    elle::unreachable();
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

      cryptography::Code const* _keypair = value._subject_keypair_0.get();
      archive << elle::serialize::pointer(_keypair);

      // Generate a key pair because the format 0 used to contain
      // the key pair in clear.
      //
      // So rather than keeping the actual key pair (which represents a
      // security breach), generate a useless key pair just for the
      // serialization mechanism to function.
      if (_keypair != nullptr)
      {
        cryptography::KeyPair random =
          cryptography::KeyPair::generate(cryptography::Cryptosystem::rsa,
                                          1024);
        cryptography::KeyPair const* _random(&random);
        archive << elle::serialize::alive_pointer(_random);
      }

      archive << value._identifier.value();
      archive << value._description;

      cryptography::Signature const* _signature(&value._signature);
      archive << elle::serialize::alive_pointer(_signature);

      break;
    }
    case 1:
    {
      archive << value._identifier;
      archive << value._issuer_K;
      archive << value._subject_K;
      archive << value._description;
      archive << value._subject_k;
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
      // Load the public key of the authority which was used at the time
      // for signing identities.
      elle::String issuer_K("AAAAAAAAAAAAAgAAwvtjO51oHrOMK/K33ajUm4lnYKWW5dUtyK5Pih7gDrtlpEPy7QWAiYjY8Kinlctca2owzcPXrvFE34gxQE/xz11KLzw4ypn4/ABdzjaTgGoCNLJ2OX99IPk6sEjIHwFxR9YcewD6uED2FQgv4OfOROHaL8hmHzRc0/BxjKwtI6fT7Y/e1v2LMig6r30abqcLrZN+v+3rPHN4hb8n1Jz7kRxRbtglFPLDpvI5LUKEGmu3FPKWWZiJsyFuuLUoC9WsheDDZoHYjyrzMD0k7Sp5YVGBBDthZc6SQDp1ktPSTou1Opk+1IxHp/we1/HNhULvGvr6B1KYZJhb/R55H0k6GcaRQmNEKgiLcF6Z9lA5asK49CC/tlZjKRkXkLBKR9zGIODsweY+O9y3AeGX+Pfk9itPals2egsxc/q2mbRaC9svY2TXMwiSO4EPiedqvpuTKj1KTcRbQrp5mSxG1nzaCGyCmUeGzoBJZLNVJHpytAfwf0oYWfo9NOD9dkKkkL5jxfW3+MOwEx4i0tP3xdKmt6wC6CSXedFVm55oOcz2YgARG3hw0vBdLtl3jxfbXAFjCNnpkMrCEMfjzs5ecFVwhmM8OEPqHpyYJYO/9ipwXAKRPugFzMvoyggSA6G5JfVEwuCgOp2v82ldsKl0sC34/mBeKrJvjaZAXm39f6jTw/sAAAMAAAABAAE=");
      elle::serialize::from_string<elle::serialize::InputBase64Archive>(
        issuer_K) >> value._issuer_K;

      cryptography::Code* keypair_pointer = nullptr;
      archive >> elle::serialize::pointer(keypair_pointer);
      value._subject_keypair_0.reset(keypair_pointer);

      if (keypair_pointer != nullptr)
      {
        cryptography::KeyPair* forget_pointer = nullptr;
        archive >> elle::serialize::alive_pointer(forget_pointer);
        std::unique_ptr<cryptography::KeyPair> forget(forget_pointer);
        // Forget the key pair as we should never have had access to
        // it from the archive anyway.
        (void)forget;
      }

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

      // Re-serialize the signature and deserialize it in the _signature
      // attribute so as to transform a pointer into a value.
      cryptography::Signature* signature_pointer = nullptr;
      archive >> elle::serialize::alive_pointer(signature_pointer);
      std::unique_ptr<cryptography::Signature> signature(signature_pointer);
      elle::String signature_archive;
      elle::serialize::to_string(signature_archive) << *signature;
      elle::serialize::from_string(signature_archive) >> value._signature;

      break;
    }
    case 1:
    {
      archive >> value._identifier;
      archive >> value._issuer_K;
      archive >> value._subject_K;
      archive >> value._description;
      archive >> value._subject_k;
      archive >> value._signature;

      break;
    }
    default:
      throw ::infinit::Exception(
        elle::sprintf("unknown format '%s'", format));
  }
}

#endif
