#ifndef INFINIT_IDENTITY_HH
# define INFINIT_IDENTITY_HH

# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/serialize/Format.hh>
# include <elle/serialize/DynamicFormat.hh>
# include <elle/serialize/construct.hh>
# include <elle/fwd.hh>

# include <cryptography/fwd.hh>
# include <cryptography/Code.hh>
# include <cryptography/Cipher.hh>
# include <cryptography/KeyPair.hh>
# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

namespace infinit
{
  /// Represent a user identity issued by an authority.
  ///
  /// The identity contains a unique identifier, a human-readable
  /// (potentially non-unique) name along with the user's key pair
  /// encrypted with a password known from the user only.
  ///
  /// The decrypt() method can be used to retrieve the actual key
  /// pair providing the given password is valid.
  class Identity:
    public elle::Printable,
    public elle::serialize::DynamicFormat<Identity>
  {
    // XXX to remove once the code has been factored: there should not
    //     be a default length.
  public:
    static elle::Natural32 const keypair_length = 2048;

    /*----------.
    | Constants |
    `----------*/
  public:
    struct Constants
    {
      static cryptography::cipher::Algorithm const cipher_algorithm;
    };

    /*-------------.
    | Construction |
    `-------------*/
  public:
    /// Construct an identity based on the given elements.
    explicit
    Identity(elle::String identifier,
             elle::String name,
             cryptography::Code code,
             cryptography::Signature signature);
    /// Construct an identity based on the passed elements which will
    /// be signed with the given authority.
    ///
    /// Note that, through this helper, the _authority_ must provide a
    /// sign(data) method which returns a signature.
    template <typename T>
    explicit
    Identity(elle::String identifier,
             elle::String name,
             cryptography::KeyPair const& pair,
             elle::String const& passphrase,
             T const& authority);
    Identity(Identity const& other);
    Identity(Identity&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Identity);
  private:
    /// An intermediate constructor which takes a secret key for encrypting
    /// the given key pair.
    template <typename T>
    explicit
    Identity(elle::String identifier,
             elle::String name,
             cryptography::KeyPair const& pair,
             cryptography::SecretKey const& key,
             T const& authority);
    /// An intermediate constructor for signing the elements with the
    /// authority.
    template <typename T>
    explicit
    Identity(elle::String identifier,
             elle::String name,
             cryptography::Code code,
             T const& authority);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Return true if the identity is valid i.e has been issued by given
    /// authority.
    ///
    /// Note that the _authority_ must provide a verify(signature, data)
    /// method.
    template <typename T>
    elle::Boolean
    validate(T const& authority) const;
    /// Return the key pair associated with the identity's user by decrypting
    /// the embedded version.
    cryptography::KeyPair
    decrypt(elle::String const& passphrase) const;

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Identity);

    /*-----------.
    | Interfaces |
    `-----------*/
  public:
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Identity);
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  private:
    /// The identifier uniquely identifying the user within a
    /// given authority's scope.
    ELLE_ATTRIBUTE_R(elle::String, identifier);
    /// A human-readable name.
    ELLE_ATTRIBUTE_R(elle::String, name);
    /// The user's key pair in its encrypted form.
    ELLE_ATTRIBUTE(cryptography::Code, code);
    /// A signature issued by the authority certifying the identity's
    /// validity.
    ELLE_ATTRIBUTE(cryptography::Signature, signature);
  };

  namespace identity
  {
    /*----------.
    | Functions |
    `----------*/

    /// Return a digest of the most fondamental elements composing an identity.
    ///
    /// These are the elements which must be signed by the authority.
    cryptography::Digest
    hash(elle::String const& identifier,
         elle::String const& name,
         cryptography::Code const& code);
  }
}

# include <infinit/Identity.hxx>

#endif
