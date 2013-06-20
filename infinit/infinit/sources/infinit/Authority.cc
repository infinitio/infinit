#include <infinit/Authority.hh>

#include <cryptography/PrivateKey.hh>
#include <cryptography/SecretKey.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.Authority")

namespace infinit
{
  /*----------.
  | Constants |
  `----------*/

  cryptography::cipher::Algorithm const
  Authority::Constants::cipher_algorithm =
    cryptography::cipher::Algorithm::aes256;

  /*-------------.
  | Construction |
  `-------------*/

  Authority::Authority(cryptography::PublicKey K,
                       elle::String description,
                       cryptography::Code k,
                       Identifier identifier):
    _identifier(std::move(identifier)),
    _K(std::move(K)),
    _description(std::move(description)),
    _k(std::move(k))
  {
  }

  Authority::Authority(cryptography::PublicKey K,
                       elle::String description,
                       cryptography::PrivateKey k,
                       elle::String const& passphrase,
                       Identifier identifier):
    Authority(std::move(K),
              std::move(description),
              std::move(k),
              cryptography::SecretKey(Authority::Constants::cipher_algorithm,
                                      passphrase),
              std::move(identifier))
  {
  }

  Authority::Authority(cryptography::PublicKey K,
                       elle::String description,
                       cryptography::PrivateKey k,
                       cryptography::SecretKey const& key,
                       Identifier identifier):
    Authority(std::move(K),
              std::move(description),
              key.encrypt(k),
              std::move(identifier))
  {
  }

  Authority::Authority(Authority const& other):
    elle::serialize::DynamicFormat<Authority>(other),
    _identifier(other._identifier),
    _K(other._K),
    _description(other._description),
    _k(other._k)
  {
  }

  Authority::Authority(Authority&& other):
    elle::serialize::DynamicFormat<Authority>(std::move(other)),
    _identifier(std::move(other._identifier)),
    _K(std::move(other._K)),
    _description(std::move(other._description)),
    _k(std::move(other._k))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Authority,
                                  _identifier, _K, _k)
  {
  }

  /*--------.
  | Methods |
  `--------*/

  cryptography::PrivateKey
  Authority::decrypt(elle::String const& passphrase) const
  {
    ELLE_TRACE_METHOD(passphrase);

    cryptography::SecretKey key(Authority::Constants::cipher_algorithm,
                                passphrase);

    try
    {
      cryptography::PrivateKey k =
        key.decrypt<cryptography::PrivateKey>(this->_k);

      return (k);
    }
    catch (cryptography::Exception const& e)
    {
      throw Exception(
        elle::sprintf("unable to decrypt the authority's private key: %s",
                      e));
    }

    elle::unreachable();
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Authority::print(std::ostream& stream) const
  {
    stream << "("
           << this->_K << ", "
           << this->_description
           << ")";
  }
}
