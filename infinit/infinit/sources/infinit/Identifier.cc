#include <infinit/Identifier.hh>

#include <cryptography/random.hh>

#include <elle/format/base64url.hh>
#include <elle/Buffer.hh>

namespace infinit
{
  /*----------.
  | Constants |
  `----------*/

  elle::Natural32 const Identifier::Constants::default_size = 48;

  /*-------------.
  | Construction |
  `-------------*/

  Identifier::Identifier():
    Identifier(Identifier::Constants::default_size)
  {}

  Identifier::Identifier(elle::Natural32 const size):
    Identifier(cryptography::random::generate<elle::Buffer>(size))
  {}

  Identifier::Identifier(elle::String const& string):
    _value(string)
  {}

  Identifier::Identifier(Identifier const& other):
    _value(other._value)
  {}

  Identifier::Identifier(Identifier&& other):
    _value(std::move(other._value))
  {}

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Identifier)
  {}

  Identifier::Identifier(elle::Buffer const& buffer):
    _value(elle::format::base64url::encode<elle::String>(buffer))
  {
  }

  /*----------.
  | Operators |
  `----------*/

  elle::Boolean
  Identifier::operator ==(Identifier const& rhs) const
  {
    return (this->_value == rhs._value);
  }

  elle::Boolean
  Identifier::operator <(Identifier const& rhs) const
  {
    return (this->_value < rhs._value);
  }

  /*----------.
  | Printable |
  `----------*/

  void
  Identifier::print(std::ostream& stream) const
  {
    stream << this->_value;
  }
}
