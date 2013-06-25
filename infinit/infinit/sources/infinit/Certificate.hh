#ifndef INFINIT_CERTIFICATE_HH
# define INFINIT_CERTIFICATE_HH

# include <elle/types.hh>
# include <elle/attribute.hh>
# include <elle/operator.hh>
# include <elle/Printable.hh>
# include <elle/serialize/Format.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/fwd.hh>
# include <cryptography/PublicKey.hh>
# include <cryptography/Signature.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <infinit/Identifier.hh>

# include <chrono>

//
// ---------- Certificate -----------------------------------------------------
//

namespace infinit
{
  namespace certificate
  {
    /*------.
    | Types |
    `------*/

    /// The list of entity which an authority may be allowed to operate
    /// e.g sign/certify.
    typedef elle::Natural16 Permissions;

    /*-------.
    | Values |
    `-------*/

    namespace permissions
    {
      extern Permissions const none;
      extern Permissions const identity;
      extern Permissions const descriptor;
      extern Permissions const passport;
      extern Permissions const certificate;
      extern Permissions const all;
    }
  }

  /// A certificate is issued by an authority and certifies that another
  /// authority, of a sub-level, is allowed to sign objects.
  ///
  /// Thus both issuer and subject reference authorities in this context.
  ///
  /// Note that the nature of the objects the certified authority is allowed
  /// to sign depends on the permissions it has been granted by the authority
  /// issuing the certificate.
  ///
  /// The signature issued by the granting authority embeds every field but
  /// its public key which will be used to verify the aforementioned signature:
  ///
  ///   issuer_signature:
  ///     | identifier
  ///     | subject_K
  ///     | description
  ///     | permissions
  ///     | valid_from
  ///     | valid_until
  class Certificate:
    public elle::Printable
  {
    /*------.
    | Types |
    `------*/
  public:
    /// The type for holding the set of certificates enabling one to
    /// verify the validity of any given certificate through a chain
    /// of verifications.
    typedef std::map<cryptography::PublicKey, Certificate> Pool;

    /*-------------.
    | Construction |
    `-------------*/
  public:
    /// Construct a certificate issued by an authority, for another
    /// sub-authority.
    explicit
    Certificate(cryptography::PublicKey issuer_K,
                cryptography::PublicKey subject_K,
                elle::String description,
                certificate::Permissions permissions,
                std::chrono::system_clock::time_point valid_from,
                std::chrono::system_clock::time_point valid_until,
                cryptography::Signature issuer_signature,
                Identifier identifier = Identifier());
    /// A helper for generating the signature automatically.
    ///
    /// Note that the _issuer_ must provide a sign(data) method
    /// which returns a signature.
    template <typename T>
    explicit
    Certificate(cryptography::PublicKey issuer_K,
                cryptography::PublicKey subject_K,
                elle::String description,
                certificate::Permissions permissions,
                std::chrono::system_clock::time_point valid_from,
                std::chrono::system_clock::time_point valid_until,
                T const& issuer,
                Identifier identifier = Identifier());
    Certificate(Certificate const& other);
    Certificate(Certificate&& other);
    ELLE_SERIALIZE_CONSTRUCT_DECLARE(Certificate);

    /*--------.
    | Methods |
    `--------*/
  public:
    /// Return true if the certificate is valid.
    ///
    /// The given _pool_ provides the certificates enabling the
    /// chain verification up to the root certificate.
    elle::Boolean
    validate(Pool const& pool) const;

    /*----------.
    | Operators |
    `----------*/
  public:
    ELLE_OPERATOR_NO_ASSIGNMENT(Certificate);

    /*-----------.
    | Interfaces |
    `-----------*/
  public:
    // serializable
    ELLE_SERIALIZE_FRIEND_FOR(Certificate);
    // printable
    virtual
    void
    print(std::ostream& stream) const;

    /*-----------.
    | Attributes |
    `-----------*/
  public:
    /// The public key of the authority which issued the certificate.
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, issuer_K);
    /// An identifier theoretically uniquely identifying the certificate.
    ELLE_ATTRIBUTE_R(Identifier, identifier);
    /// The public key of the certified authority i.e subject authority.
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, subject_K);
    /// A human-readable description of the certificate's purpose.
    ELLE_ATTRIBUTE_R(elle::String, description);
    /// The set of permissions granted to the certified authority.
    ELLE_ATTRIBUTE_R(certificate::Permissions, permissions);
    /// The date from which the certificate is valid.
    ELLE_ATTRIBUTE_R(std::chrono::system_clock::time_point, valid_from);
    /// The date after which the certificate can be considered invalid.
    ELLE_ATTRIBUTE_R(std::chrono::system_clock::time_point, valid_until);
    /// The signature issued by the issuer authority.
    ELLE_ATTRIBUTE(cryptography::Signature, issuer_signature);
  };

  /*----------.
  | Operators |
  `----------*/

  std::ostream&
  operator <<(std::ostream& stream,
              Certificate::Pool const& pool);

  namespace certificate
  {
    /*----------.
    | Functions |
    `----------*/

    /// Return a digest of the most fondamental elements composing the
    /// certificate.
    ///
    /// These are the elements which must be signed by the issuer authority.
    cryptography::Digest
    hash(Identifier const& identifier,
         cryptography::PublicKey const& subject_K,
         elle::String const& description,
         certificate::Permissions const& permissions,
         std::chrono::system_clock::time_point const& valid_from,
         std::chrono::system_clock::time_point const& valid_until);
  }
}

//
// ---------- Pool ------------------------------------------------------------
//

namespace infinit
{
  namespace certificate
  {
    /// XXX
    class Pool
    {
      // XXX
    };
  }
}

# include <infinit/Certificate.hxx>

#endif
