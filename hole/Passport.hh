#ifndef ELLE_PASSPORT_HH
# define ELLE_PASSPORT_HH

# include <elle/attribute.hh>
# include <elle/Printable.hh>
# include <elle/concept/Fileable.hh>
# include <elle/concept/Uniquable.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/Signature.hh>

# include <hole/Authority.hh>

// XXX[temporary: for cryptography]
using namespace infinit;

namespace elle
{
  ///
  /// this class uniquely identify a device through a label which is
  /// used by the storage layer to locate the nodes responsible for a
  /// block's replica for instance.
  ///
  class Passport:
    public elle::concept::MakeFileable<Passport>,
    public elle::concept::MakeUniquable<Passport>,
    public elle::Printable
  {
  private:
    ELLE_ATTRIBUTE_R(elle::String, id);
    ELLE_ATTRIBUTE_R(elle::String, name);
    ELLE_ATTRIBUTE_R(cryptography::PublicKey, owner_K);
    cryptography::Signature _signature;

    /*-------------.
    | Construction |
    `-------------*/
  public:
    Passport(); // XXX Do not use (sheduled for deletion)
    // XXX[init the complex attributes: owner & signature]
    ELLE_SERIALIZE_CONSTRUCT(Passport) {}
    /// @param id Unique identifier.
    /// @param name Represented device name.
    /// @param owner_K Owning user keys.
    /// @param
    Passport(elle::String const& id,
             elle::String const& name,
             cryptography::PublicKey const& owner_K,
             elle::Authority const& authority);

  public:
    /// Check the passport signature.
    bool
    validate(elle::Authority const&) const;

    /// Dump the passport.
    void
    dump(const elle::Natural32 = 0) const;

    /// Passport is fileable.
    ELLE_CONCEPT_FILEABLE_METHODS();

    void
    print(std::ostream& stream) const;

    bool operator == (Passport const&) const;

  private:
    ELLE_SERIALIZE_FRIEND_FOR(Passport);
  };
}

# include <hole/Passport.hxx>

#endif
