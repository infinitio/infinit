#ifndef HOLE_PASSPORT_HH
# define HOLE_PASSPORT_HH

# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/serialize/DynamicFormat.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/Signature.hh>
# include <cryptography/PublicKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

// XXX si on depend sur Identifier, autant mettre Passport dans infinit avec
//     Identity, Descriptor etc.
# include <infinit/Identifier.hh>

ELLE_OPERATOR_RELATIONALS();

namespace hole
{
  /// A passport uniquely identify a device as belonging to a user.
  ///
  /// Note that this structure benefits from a double signature. The
  /// owner first certifies having issued this passport for one of
  /// its devices. Then the authority certifies allowing such a passport
  /// to be used to connect to other devices belonging to the same
  /// namespace.
  ///
  /// The owner's signature therefore includes the following attributes:
  ///
  ///   owner_signature:
  ///     | identifier
  ///     | description
  ///
  /// Then, the authority's signature embeds:
  ///
  ///   authority_signature:
  ///     | owner_K
  ///     | owner_signature
  class Passport:
    public elle::serialize::DynamicFormat<Passport>,
    public elle::Printable
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    Passport(); // XXX to remove
    /// Construct a passport based on the given elements.
    Passport(cryptography::PublicKey authority_K,
             cryptography::PublicKey owner_K,
             elle::String description,
             cryptography::Signature owner_signature,
             cryptography::Signature authority_signature,
             Identifier identifier = Identifier());
    /// Construct a passport based on the passed elements which will
    /// be signed with the owner before being sealed with the given
    /// authority.
    ///
    /// Note that, through this helper, both the _owner_ and the
    /// _authority_ must provide a sign(data) method which returns a
    /// signature.
    template <typename O,
              typename A>
    Passport(cryptography::PublicKey authority_K,
             cryptography::PublicKey owner_K,
             elle::String description,
             O const& owner,
             A const& authority,
             Identifier identifier = Identifier());
    Passport(Passport const& other);
    Passport(Passport&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Passport);
  private:
    /// An intermediate constructor.
    template <typename A>
    Passport(cryptography::PublicKey authority_K,
             cryptography::PublicKey owner_K,
             elle::String description,
             cryptography::Signature owner_signature,
             A const& authority,
             Identifier identifier);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Verify the passport's validity.
    ///
    /// Note that the _authority_ must provide a verify(signature, data)
    /// method.
    template <typename T>
    elle::Boolean
    validate(T const& authority) const;

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Passport);
    elle::Boolean
    operator ==(Passport const& other) const;
    elle::Boolean
    operator <(Passport const& other) const;

    /*-----------.
    | Interfaces |
    `-----------*/
  private:
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Passport);
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  private:
    /// The public key of the authority which issued the passport.
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, authority_K);
    /// The public key of the owner of the device which the passport identifies.
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, owner_K);
    /// A theoretically unique identifier which can be used to distinguish
    /// passports.
    ELLE_ATTRIBUTE_R(Identifier, identifier);
    /// A description of the passport's purpose such as the device name or else.
    ///
    /// Note that this description is not confidential since passports are
    /// distributed among the nodes of the network so as to authenticate one
    /// another.
    ELLE_ATTRIBUTE_R(elle::String, description);
    /// A signature issued by the owner of the passport so as to guarantee
    /// the owner authorised the associated device.
    ELLE_ATTRIBUTE(cryptography::Signature, owner_signature);
    /// A signature issued by the authority allowed to certify such passports.
    ELLE_ATTRIBUTE(cryptography::Signature, authority_signature);
  };

  namespace passport
  {
    /*----------.
    | Functions |
    `----------*/

    /// Return a digest of the most fondamental elements composing a passport.
    ///
    /// These are the elements which must be signed by the owner.
    cryptography::Digest
    hash(Identifier const& identifier,
         elle::String const& description);
    /// Return a digest of the elements identifying the owner and its signature.
    ///
    /// These are the elements which must be signed by the authority.
    cryptography::Digest
    hash(cryptography::PublicKey const& owner_K,
         cryptography::Signature const& owner_signature);
    /// Compatibility with format 0.
    cryptography::Digest
    hash_0(elle::String const& identifier,
           cryptography::PublicKey const& owner_K);
  }
}

# include <hole/Passport.hxx>

#endif
