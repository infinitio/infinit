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

  Authority::Authority(elle::String description,
                       cryptography::PublicKey K,
                       cryptography::PrivateKey k,
                       elle::String const& passphrase):
    Authority(std::move(description),
              std::move(K),
              std::move(k),
              cryptography::SecretKey(Authority::Constants::cipher_algorithm,
                                      passphrase))
  {
  }

  Authority::Authority(elle::String description,
                       cryptography::PublicKey K,
                       cryptography::PrivateKey k,
                       cryptography::SecretKey const& key):
    _description(std::move(description)),
    _K(std::move(K)),
    _k(key.encrypt(k))
  {
  }

  Authority::Authority(Authority const& other):
    elle::serialize::DynamicFormat<Authority>(other),
    _description(other._description),
    _K(other._K),
    _k(other._k)
  {
  }

  Authority::Authority(Authority&& other):
    elle::serialize::DynamicFormat<Authority>(std::move(other)),
    _description(std::move(other._description)),
    _K(std::move(other._K)),
    _k(std::move(other._k))
  {
  }

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Authority,
                                  _K, _k)
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

    return (key.decrypt<cryptography::PrivateKey>(this->_k));
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Authority::print(std::ostream& stream) const
  {
    stream << "("
           << this->_description << ", "
           << this->_K
           << ")";
  }
}
