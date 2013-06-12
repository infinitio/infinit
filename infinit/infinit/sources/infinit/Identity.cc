#include <infinit/Identity.hh>

#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/KeyPair.hh>
#include <cryptography/Code.hh>
#include <cryptography/SecretKey.hh>

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

  Identity::Identity(elle::String identifier,
                     elle::String name,
                     cryptography::Code keypair,
                     cryptography::Signature signature):
    _identifier(std::move(identifier)),
    _name(std::move(name)),
    _keypair(std::move(keypair)),
    _signature(std::move(signature))
  {
  }

  Identity::Identity(Identity const& other):
    elle::serialize::DynamicFormat<Identity>(other),
    _identifier(other._identifier),
    _name(other._name),
    _keypair(other._keypair),
    _signature(other._signature)
  {
  }

  Identity::Identity(Identity&& other):
    elle::serialize::DynamicFormat<Identity>(std::move(other)),
    _identifier(std::move(other._identifier)),
    _name(std::move(other._name)),
    _keypair(std::move(other._keypair)),
    _signature(std::move(other._signature))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Identity,
                                  _keypair, _signature)
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

    return (key.decrypt<cryptography::KeyPair>(this->_keypair));
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Identity::print(std::ostream& stream) const
  {
    stream << "("
           << this->_identifier << ", "
           << this->_name << ", "
           << this->_keypair
           << ")";
  }

  namespace identity
  {
    /*----------.
    | Functions |
    `----------*/

    cryptography::Digest
    hash(elle::String const& identifier,
         elle::String const& name,
         cryptography::Code const& keypair)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  name,
                  keypair),
                cryptography::KeyPair::oneway_algorithm));
    }
  }
}
