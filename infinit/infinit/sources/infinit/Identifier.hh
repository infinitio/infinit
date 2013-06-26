#ifndef INFINIT_IDENTIFIER_HH
# define INFINIT_IDENTIFIER_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/fwd.hh>

# include <elle/serialize/construct.hh>

ELLE_OPERATOR_RELATIONALS();

namespace infinit
{
  /// Represent a theoretically unique identifier which can be used
  /// to identify entities.
  class Identifier:
    public elle::Printable
  {
    /*----------.
    | Constants |
    `----------*/
  public:
    struct Constants
    {
      static elle::Natural32 const default_size;
    };

    /*-------------.
    | Construction |
    `-------------*/
  public:
    /// Construct a random identifier out of _size_ bytes.
    explicit
    Identifier(elle::Natural32 const size = default_size);
    /// Construct an identifier based on a string.
    explicit
    Identifier(elle::String const& string);
    Identifier(Identifier const& other);
    Identifier(Identifier&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Identifier);
  private:
    /// An intermediate constructor following the generation of a buffer.
    explicit
    Identifier(elle::Buffer const& buffer);

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Identifier);
    elle::Boolean
    operator ==(Identifier const& rhs) const;
    elle::Boolean
    operator <(Identifier const& rhs) const;

    /*-----------.
    | Interfaces |
    `-----------*/
  public:
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Identifier);
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  private:
    ELLE_ATTRIBUTE_R(elle::String, value);
  };
}

namespace std
{
  /*---------------------.
  | Type Specializations |
  `---------------------*/

  template <>
  struct hash<::infinit::Identifier>;
}

# include <infinit/Identifier.hxx>

#endif
