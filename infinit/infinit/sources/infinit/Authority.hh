#ifndef INFINIT_AUTHORITY_HH
# define INFINIT_AUTHORITY_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/serialize/Format.hh>
# include <elle/serialize/DynamicFormat.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/fwd.hh>
# include <cryptography/Code.hh>
# include <cryptography/PublicKey.hh>
# include <cryptography/PrivateKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <infinit/Identifier.hh>

namespace infinit
{
  /// Represent an authority capable of signing objects such as identities,
  /// descriptors, certificates and so on.
  ///
  /// Given the extreme sensitivity of the information, especially the
  /// key pair, these information are encrypted with a passphrase known to
  /// the authority owner only.
  class Authority:
    public elle::Printable,
    public elle::serialize::DynamicFormat<Authority>
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
    /// Construct an authority given a description and, mosty importantly,
    /// the key pair and the passphrase for encrypting it.
    explicit
    Authority(cryptography::PublicKey K,
              elle::String description,
              cryptography::Code k,
              Identifier identifier = Identifier());
    /// Construct and encrypt the given private key with the passphrase.
    explicit
    Authority(cryptography::PublicKey K,
              elle::String description,
              cryptography::PrivateKey k,
              elle::String const& passphrase,
              Identifier identifier = Identifier());
    Authority(Authority const& other);
    Authority(Authority&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Authority);
  private:
    /// Intermediate secret-key-based constructor.
    explicit
    Authority(cryptography::PublicKey K,
              elle::String description,
              cryptography::PrivateKey k,
              cryptography::SecretKey const& key,
              Identifier identifier);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Return the authority's private key in its decrypted form.
    cryptography::PrivateKey
    decrypt(elle::String const& passphrase) const;

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Authority);

    /*-----------.
    | Interfaces |
    `-----------*/
  public:
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Authority);
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  public:
    /// An identifier theoretically uniquely identifying the authority.
    ELLE_ATTRIBUTE_R(Identifier, identifier);
    /// The public key of the authority.
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, K);
    /// A description of the purpose of this authority.
    ELLE_ATTRIBUTE_R(elle::String, description);
    /// The authority's private key which is encrypted with
    /// a passphrase known to the owner only.
    ELLE_ATTRIBUTE(cryptography::Code, k);
  };
}

# include <infinit/Authority.hxx>

#endif
