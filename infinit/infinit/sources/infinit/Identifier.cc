#include <infinit/Identifier.hh>

#include <cryptography/random.hh>

#include <elle/format/hexadecimal.hh>
#include <elle/Buffer.hh>

namespace infinit
{
  /*----------.
  | Constants |
  `----------*/

  elle::Natural32 const Identifier::Constants::default_size = 64;

  /*-------------.
  | Construction |
  `-------------*/

  Identifier::Identifier():
    Identifier(Identifier::Constants::default_size)
  {}

  Identifier::Identifier(elle::Natural32 const size):
    _value(elle::format::hexadecimal::encode(
             cryptography::random::generate<elle::Buffer>(size)))
  {}

  Identifier::Identifier(elle::String const& string):
    Identifier(elle::ConstWeakBuffer(string.c_str(), string.length()))
  {}

  Identifier::Identifier(elle::ConstWeakBuffer const& buffer):
    _value(elle::format::hexadecimal::encode(buffer))
  {}

  Identifier::Identifier(Identifier const& other):
    _value(other._value)
  {}

  Identifier::Identifier(Identifier&& other):
    _value(std::move(other._value))
  {}

  ELLE_SERIALIZE_CONSTRUCT_DEFINE(Identifier)
  {}

  /*----------.
  | Printable |
  `----------*/

  void
  Identifier::print(std::ostream& stream) const
  {
    stream << this->_value;
  }
}
