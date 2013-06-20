#include <infinit/Identity.hh>

#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/KeyPair.hh>
#include <cryptography/Code.hh>
#include <cryptography/SecretKey.hh>
#include <cryptography/Exception.hh>

ELLE_LOG_COMPONENT("infinit.Identity");

namespace infinit
{
  /*----------.
  | Constants |
  `----------*/

  cryptography::cipher::Algorithm const Identity::Constants::cipher_algorithm =
    cryptography::cipher::Algorithm::aes256;

  /*-------------.
  | Construction |
  `-------------*/

  Identity::Identity(cryptography::PublicKey issuer_K,
                     cryptography::PublicKey subject_K,
                     elle::String description,
                     cryptography::Code subject_k,
                     cryptography::Signature signature,
                     Identifier identifier):
    _identifier(std::move(identifier)),
    _issuer_K(std::move(issuer_K)),
    _subject_K(std::move(subject_K)),
    _description(std::move(description)),
    _subject_k(std::move(subject_k)),
    _signature(std::move(signature))
  {
  }

  Identity::Identity(Identity const& other):
    elle::serialize::DynamicFormat<Identity>(other),
    _identifier(other._identifier),
    _issuer_K(other._issuer_K),
    _subject_K(other._subject_K),
    _description(other._description),
    _subject_k(other._subject_k),
    _signature(other._signature),
    _subject_keypair_0(new cryptography::Code(*other._subject_keypair_0))
  {
  }

  Identity::Identity(Identity&& other):
    elle::serialize::DynamicFormat<Identity>(std::move(other)),
    _identifier(std::move(other._identifier)),
    _issuer_K(std::move(other._issuer_K)),
    _subject_K(std::move(other._subject_K)),
    _description(std::move(other._description)),
    _subject_k(std::move(other._subject_k)),
    _signature(std::move(other._signature)),
    _subject_keypair_0(std::move(other._subject_keypair_0))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Identity,
                                  _identifier, _issuer_K, _subject_K,
                                  _subject_k, _signature)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  cryptography::PrivateKey
  Identity::decrypt(elle::String const& passphrase) const
  {
    ELLE_TRACE_METHOD(passphrase);

    cryptography::SecretKey key(Identity::Constants::cipher_algorithm,
                                passphrase);

    switch (this->infinit::Identity::DynamicFormat::version())
    {
      case 0:
      {
        try
        {
          ELLE_ASSERT_NEQ(this->_subject_keypair_0, nullptr);

          cryptography::KeyPair keypair =
            key.decrypt<cryptography::KeyPair>(*this->_subject_keypair_0);

          return (keypair.k());
        }
        catch (cryptography::Exception const& e)
        {
          throw Exception(
            elle::sprintf("unable to decrypt the identity's key pair: %s", e));
        }

        break;
      }
      case 1:
      {
        try
        {
          cryptography::PrivateKey k =
            key.decrypt<cryptography::PrivateKey>(this->_subject_k);

          return (k);
        }
        catch (cryptography::Exception const& e)
        {
          throw Exception(
            elle::sprintf("unable to decrypt the identity's private key: %s",
                          e));
        }

        break;
      }
      default:
        throw ::infinit::Exception(
          elle::sprintf(
            "unknown format '%s'",
            this->infinit::Identity::DynamicFormat::version()));
    }

    elle::unreachable();
  }

  cryptography::KeyPair
  Identity::decrypt_0(elle::String const& passphrase) const
  {
    ELLE_TRACE_METHOD(passphrase);

    cryptography::SecretKey key(Identity::Constants::cipher_algorithm,
                                passphrase);

    switch (this->infinit::Identity::DynamicFormat::version())
    {
      case 0:
      {
        try
        {
          ELLE_ASSERT_NEQ(this->_subject_keypair_0, nullptr);

          cryptography::KeyPair keypair =
            key.decrypt<cryptography::KeyPair>(*this->_subject_keypair_0);

          return (keypair);
        }
        catch (cryptography::Exception const& e)
        {
          throw Exception(
            elle::sprintf("unable to decrypt the identity's key pair: %s", e));
        }

        break;
      }
      case 1:
      {
        try
        {
          cryptography::PrivateKey k =
            key.decrypt<cryptography::PrivateKey>(this->_subject_k);

          return (cryptography::KeyPair(this->_subject_K, k));
        }
        catch (cryptography::Exception const& e)
        {
          throw Exception(
            elle::sprintf("unable to decrypt the identity's private key: %s",
                          e));
        }

        break;
      }
      default:
        throw ::infinit::Exception(
          elle::sprintf(
            "unknown format '%s'",
            this->infinit::Identity::DynamicFormat::version()));
    }

    elle::unreachable();
  }

  cryptography::PublicKey
  Identity::subject_K() const
  {
    switch (this->infinit::Identity::DynamicFormat::version())
    {
      case 0:
        throw Exception("unable to retrieve the subject's public key from an "
                        "identity in format 0; instead use the decrypt_0() "
                        "method which returns a key pair");
      case 1:
        return (this->_subject_K);
      default:
        throw ::infinit::Exception(
          elle::sprintf(
            "unknown format '%s'",
            this->infinit::Identity::DynamicFormat::version()));
    }

    elle::unreachable();
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Identity::print(std::ostream& stream) const
  {
    switch (this->infinit::Identity::DynamicFormat::version())
    {
      case 0:
      {
        stream << "("
               << this->_identifier << ", "
               << this->_description
               << ")";

        break;
      }
      case 1:
      {
        stream << "("
               << this->_subject_K << ", "
               << this->_description
               << ")";

        break;
      }
      default:
        throw ::infinit::Exception(
          elle::sprintf(
            "unknown format '%s'",
            this->infinit::Identity::DynamicFormat::version()));
    }
  }

  namespace identity
  {
    /*----------.
    | Functions |
    `----------*/

    cryptography::Digest
    hash(Identifier const& identifier,
         cryptography::PublicKey const& issuer_K,
         cryptography::PublicKey const& subject_K,
         elle::String const& description,
         cryptography::Code const& subject_k)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  issuer_K,
                  subject_K,
                  description,
                  subject_k),
                cryptography::KeyPair::oneway_algorithm));
    }

    cryptography::Digest
    hash_0(elle::String const& identifier,
           elle::String const& description,
           cryptography::Code const& keypair)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  description,
                  keypair),
                cryptography::KeyPair::oneway_algorithm));
    }
  }
}
