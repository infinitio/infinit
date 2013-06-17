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
# include <cryptography/PublicKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

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
    public elle::serialize::DynamicFormat<Identity>
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    /// Construct an authority given a description and, mosty importantly,
    /// the key pair and the passphrase for encrypting it.
    Authority(elle::String description,
              cryptography::KeyPair const& keypair,
              elle::String const& passphrase);
    Authority(Authority const& other);
    Authority(Authority&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Authority);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Return the key pair in its decrypted form.
    cryptography::KeyPair
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
    Type                type;

    ELLE_ATTRIBUTE_R(cryptography::PublicKey, K);
  private:
    cryptography::PrivateKey* _k;
    cryptography::Code* _code;
    ELLE_ATTRIBUTE(elle::String, description);
    ELLE_ATTRIBUTE(


  public:
    cryptography::PrivateKey const&
    k() const;
    cryptography::Code const&
    code() const;

  };

}

# include <infinit/Authority.hxx>

#endif
