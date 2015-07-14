#ifndef PAPIER_AUTHORITY_HH
# define PAPIER_AUTHORITY_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/io/Dumpable.hh>
# include <elle/concept/Fileable.hh>

# include <cryptography/fwd.hh>
# include <cryptography/rsa/PublicKey.hh>

namespace papier
{
  /// This class represents the authority.
  ///
  /// The authority can be represented through its whole key pair through
  /// which it can both sign and verify signatures or only through the
  /// public key, the most common case, through which it is only used to
  /// verify signatures.
  class Authority:
    public elle::concept::MakeFileable<Authority>,
    public elle::io::Dumpable
  {
    /*-------------.
    | Enumerations |
    `-------------*/
  public:
    enum Type
      {
        TypeUnknown,
        TypePair,
        TypePublic
      };

  /*-------------.
  | Construction |
  `-------------*/
  public:
    /// Create an authority based on the given key pair.
    Authority(infinit::cryptography::rsa::KeyPair const&);
    /// Create an authority based on the given public key only.
    Authority(infinit::cryptography::rsa::PublicKey const&);
    /// Create a copy of an Authority.
    Authority(Authority const& from);
    /// Deserialize an Authority.
    Authority(elle::io::Path const& path);
    /// Create an authority using the Inifnit public key
    Authority();
    /// Dispose of an Authority.
    ~Authority();

    Authority& operator=(Authority&&) = default;

    //
    // methods
    //

    elle::Status        Encrypt(const elle::String&);
    elle::Status        Decrypt(const elle::String&);

    //
    // static methods
    //
  private:

    //
    // interfaces
    //
  public:
    // dumpable
    elle::Status        Dump(const elle::Natural32 = 0) const;
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Authority);
    // fileable
    ELLE_CONCEPT_FILEABLE_METHODS();

    //
    // attributes
    //
    Type                type;

    ELLE_ATTRIBUTE_R(infinit::cryptography::rsa::PublicKey, K);
  private:
    ELLE_ATTRIBUTE(infinit::cryptography::rsa::PrivateKey*, k);
    ELLE_ATTRIBUTE(infinit::cryptography::Code*, code);

  public:
    infinit::cryptography::rsa::PrivateKey const&
    k() const;
    infinit::cryptography::Code const&
    code() const;
  };


  /// Infinit authority public key.
  extern std::string const key;

  /// Infinit authority.
  papier::Authority
  authority();
}

#include <papier/Authority.hxx>

#endif
