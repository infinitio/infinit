#include <hole/Passport.hh>

#include <elle/serialize/TupleSerializer.hxx>

#include <cryptography/KeyPair.hh>

namespace hole
{
  /*-------------.
  | Construction |
  `-------------*/

  Passport::Passport()
  {}

  Passport::Passport(cryptography::PublicKey authority_K,
                     cryptography::PublicKey owner_K,
                     elle::String description,
                     cryptography::Signature owner_signature,
                     cryptography::Signature authority_signature,
                     Identifier identifier):
    _authority_K(std::move(authority_K)),
    _identifier(std::move(identifier)),
    _owner_K(std::move(owner_K)),
    _description(std::move(description)),
    _owner_signature(std::move(owner_signature)),
    _authority_signature(std::move(authority_signature))
  {}

  Passport::Passport(Passport const& other):
    elle::serialize::DynamicFormat<Passport>(other),
    _authority_K(other._authority_K),
    _identifier(other._identifier),
    _owner_K(other._owner_K),
    _description(other._description),
    _owner_signature(other._owner_signature),
    _authority_signature(other._authority_signature)
  {}

  Passport::Passport(Passport&& other):
    elle::serialize::DynamicFormat<Passport>(std::move(other)),
    _authority_K(std::move(other._authority_K)),
    _identifier(std::move(other._identifier)),
    _owner_K(std::move(other._owner_K)),
    _description(std::move(other._description)),
    _owner_signature(std::move(other._owner_signature)),
    _authority_signature(std::move(other._authority_signature))
  {}

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Passport,
                                  _authority_K, _identifier, _owner_K,
                                  _owner_signature, _authority_signature)
  {}

  /*----------.
  | Operators |
  `----------*/

  elle::Boolean
  Passport::operator ==(Passport const& other) const
  {
    return (this->_identifier == other._identifier);
  }

  elle::Boolean
  Passport::operator <(Passport const& other) const
  {
    return (this->_identifier < other._identifier);
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Passport::print(std::ostream& stream) const
  {
    stream << "(" << this->_identifier << ", " << this->_description << ")";
  }

  namespace passport
  {
    /*----------.
    | Functions |
    `----------*/

    cryptography::Digest
    hash(elle::String const& description)
    {
      return (cryptography::oneway::hash(
                description,
                cryptography::KeyPair::oneway_algorithm));
    }

    cryptography::Digest
    hash(Identifier const& identifier,
         cryptography::PublicKey const& owner_K,
         cryptography::Signature const& owner_signature)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  owner_K,
                  owner_signature),
                cryptography::KeyPair::oneway_algorithm));
    }

    cryptography::Digest
    hash_0(elle::String const& identifier,
           cryptography::PublicKey const& owner_K)
    {
      return (cryptography::oneway::hash(
                elle::serialize::make_tuple(
                  identifier,
                  owner_K),
                cryptography::KeyPair::oneway_algorithm));
    }
  }
}
