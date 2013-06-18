#ifndef INFINIT_IDENTIFIER_HH
# define INFINIT_IDENTIFIER_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/fwd.hh>

namespace infinit
{
  /// XXX
  class Identifier:
    public elle::Printable
  {
    /*----------.
    | Constants |
    `----------*/
  public:
    struct Constants
    {
      static elle::Natural const default_size;
    };

    /*-------------.
    | Construction |
    `-------------*/
  public:
    explicit
    Identifier();
    /// Construct a random identifier out of _size_ bytes.
    explicit
    Identifier(elle::Natural32 const size);
    /// Construct an identifier based on a string.
    explicit
    Identifier(elle::String const& string);
    /// Construct an identifier based on a buffer.
    explicit
    Identifier(elle::ConstWeakBuffer const& buffer);
    Identifier(Identifier const& other);
    Identifier(Identifier&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Identifier);

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Identifier);

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

#endif
