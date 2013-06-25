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

  Identity::Identity(cryptography::PublicKey authority_K,
                     cryptography::PublicKey user_K,
                     elle::String description,
                     cryptography::Code user_k,
                     cryptography::Signature authority_signature,
                     Identifier identifier):
    _authority_K(std::move(authority_K)),
    _identifier(std::move(identifier)),
    _user_K(std::move(user_K)),
    _description(std::move(description)),
    _user_k(std::move(user_k)),
    _authority_signature(std::move(authority_signature))
  {
  }

  Identity::Identity(Identity const& other):
    elle::serialize::DynamicFormat<Identity>(other),
    _authority_K(other._authority_K),
    _identifier(other._identifier),
    _user_K(other._user_K),
    _description(other._description),
    _user_k(other._user_k),
    _authority_signature(other._authority_signature),
    _user_keypair_0(new cryptography::Code(*other._user_keypair_0))
  {
  }

  Identity::Identity(Identity&& other):
    elle::serialize::DynamicFormat<Identity>(std::move(other)),
    _authority_K(std::move(other._authority_K)),
    _identifier(std::move(other._identifier)),
    _user_K(std::move(other._user_K)),
    _description(std::move(other._description)),
    _user_k(std::move(other._user_k)),
    _authority_signature(std::move(other._authority_signature)),
    _user_keypair_0(std::move(other._user_keypair_0))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Identity,
                                  _authority_K, _identifier, _user_K,
                                  _user_k, _authority_signature)
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
          ELLE_ASSERT_NEQ(this->_user_keypair_0, nullptr);

          cryptography::KeyPair keypair =
            key.decrypt<cryptography::KeyPair>(*this->_user_keypair_0);

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
            key.decrypt<cryptography::PrivateKey>(this->_user_k);

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
          ELLE_ASSERT_NEQ(this->_user_keypair_0, nullptr);

          cryptography::KeyPair keypair =
            key.decrypt<cryptography::KeyPair>(*this->_user_keypair_0);

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
            key.decrypt<cryptography::PrivateKey>(this->_user_k);

          return (cryptography::KeyPair(this->_user_K, k));
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
  Identity::user_K() const
  {
    switch (this->infinit::Identity::DynamicFormat::version())
    {
      case 0:
        throw Exception("unable to retrieve the user's public key from an "
                        "identity in format 0; instead use the decrypt_0() "
                        "method which returns a key pair");
      case 1:
        return (this->_user_K);
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
               << this->_user_K << ", "
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
         cryptography::PublicKey const& user_K,
         elle::String const& description,
         cryptography::Code const& user_k)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  user_K,
                  description,
                  user_k),
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
