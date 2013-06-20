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
# include <cryptography/cipher.hh>
# include <cryptography/KeyPair.hh>
# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <infinit/Identifier.hh>

namespace infinit
{
  /// Represent a user identity issued by an authority.
  ///
  /// The identity contains a unique identifier, a human-readable
  /// description along with the user's key pair encrypted with a
  /// password known from the user only.
  ///
  /// The decrypt() method can be used to retrieve the actual key
  /// pair providing the given password is valid.
  class Identity:
    public elle::Printable,
    public elle::serialize::DynamicFormat<Identity>
  {
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
    Identity(cryptography::PublicKey issuer_K,
             cryptography::PublicKey subject_K,
             elle::String description,
             cryptography::Code subject_k,
             cryptography::Signature signature,
             Identifier identifier = Identifier());
    /// Construct an identity based on the passed elements which will
    /// be signed with the given authority.
    ///
    /// Note that, through this helper, the _authority_ must provide a
    /// sign(data) method which returns a signature.
    template <typename T>
    explicit
    Identity(cryptography::PublicKey issuer_K,
             cryptography::PublicKey subject_K,
             elle::String description,
             cryptography::PrivateKey const& subject_k,
             elle::String const& passphrase,
             T const& authority,
             Identifier identifier = Identifier());
    Identity(Identity const& other);
    Identity(Identity&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Identity);
  private:
    /// An intermediate constructor which takes a secret key for encrypting
    /// the given key pair.
    template <typename T>
    explicit
    Identity(cryptography::PublicKey issuer_K,
             cryptography::PublicKey subject_K,
             elle::String description,
             cryptography::PrivateKey const& subject_k,
             cryptography::SecretKey const& key,
             T const& authority,
             Identifier identifier);
    /// An intermediate constructor for signing the elements with the
    /// authority.
    template <typename T>
    explicit
    Identity(cryptography::PublicKey issuer_K,
             cryptography::PublicKey subject_K,
             elle::String description,
             cryptography::Code subject_k,
             T const& authority,
             Identifier identifier);

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
    /// Return the private key associated with the identity's user by
    /// decrypting the embedded code.
    cryptography::PrivateKey
    decrypt(elle::String const& passphrase) const;
    /// For compatibility with format 0.
    cryptography::KeyPair
    decrypt_0(elle::String const& passphrase) const;
    /// For compatibility with format 0.
    cryptography::PublicKey
    subject_K() const;

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
    /// An identifier theoretically uniquely identifying the identity.
    ELLE_ATTRIBUTE_R(Identifier, identifier);
    /// The public key of the issuing authority.
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, issuer_K);
    /// The public key of the user associated with this identity.
    ELLE_ATTRIBUTE(cryptography::PublicKey, subject_K);
    /// A human-readable description of the user. This string could include
    /// the real name, email, postal address, country or whatever information
    /// that the user wishes to share with the other users of a network.
    ELLE_ATTRIBUTE_R(elle::String, description);
    /// The user's private key in its encrypted form.
    ELLE_ATTRIBUTE(cryptography::Code, subject_k);
    /// A signature issued by the authority certifying the identity's
    /// validity.
    ELLE_ATTRIBUTE(cryptography::Signature, signature);

    /// Compatibility with format 0.
    ELLE_ATTRIBUTE(std::unique_ptr<cryptography::Code>, subject_keypair_0);
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
    hash(Identifier const& identifier,
         cryptography::PublicKey const& issuer_K,
         cryptography::PublicKey const& subject_K,
         elle::String const& description,
         cryptography::Code const& subject_k);
    /// Compatibility with format 0.
    cryptography::Digest
    hash_0(elle::String const& identifier,
           elle::String const& description,
           cryptography::Code const& keypair);
  }
}

# include <infinit/Identity.hxx>

#endif
