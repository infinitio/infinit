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
                     elle::String identifier,
                     elle::String description,
                     cryptography::Code keypair,
                     cryptography::Signature signature):
    _issuer_K(std::move(issuer_K)),
    _identifier(std::move(identifier)),
    _description(std::move(description)),
    _keypair(std::move(keypair)),
    _signature(std::move(signature))
  {
  }

  Identity::Identity(Identity const& other):
    elle::serialize::DynamicFormat<Identity>(other),
    _issuer_K(other._issuer_K),
    _identifier(other._identifier),
    _description(other._description),
    _keypair(other._keypair),
    _signature(other._signature)
  {
  }

  Identity::Identity(Identity&& other):
    elle::serialize::DynamicFormat<Identity>(std::move(other)),
    _issuer_K(std::move(other._issuer_K)),
    _identifier(std::move(other._identifier)),
    _description(std::move(other._description)),
    _keypair(std::move(other._keypair)),
    _signature(std::move(other._signature))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Identity,
                                  _issuer_K, _keypair, _signature)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  cryptography::KeyPair
  Identity::decrypt(elle::String const& passphrase) const
  {
    ELLE_TRACE_METHOD(passphrase);

    cryptography::SecretKey key(Identity::Constants::cipher_algorithm,
                                passphrase);

    try
    {
      cryptography::KeyPair keypair =
        key.decrypt<cryptography::KeyPair>(this->_keypair);

      return (keypair);
    }
    catch (cryptography::Exception const& e)
    {
      throw Exception(elle::sprintf("unable to decrypt the identity: %s", e));
    }

    elle::unreachable();
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Identity::print(std::ostream& stream) const
  {
    stream << "("
           << this->_identifier << ", "
           << this->_description << ", "
           << this->_keypair
           << ")";
  }

  namespace identity
  {
    /*----------.
    | Functions |
    `----------*/

    cryptography::Digest
    hash(cryptography::PublicKey const& issuer_K,
         elle::String const& identifier,
         elle::String const& description,
         cryptography::Code const& keypair)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  issuer_K,
                  identifier,
                  description,
                  keypair),
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
