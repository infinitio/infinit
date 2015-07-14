#ifndef PAPIER_PASSPORT_HH
# define PAPIER_PASSPORT_HH

# include <elle/attribute.hh>
# include <elle/Printable.hh>
# include <elle/concept/Fileable.hh>
# include <elle/concept/Uniquable.hh>
# include <elle/serialize/construct.hh>

# include <cryptography/_legacy/Signature.hh>

# include <papier/Authority.hh>

namespace papier
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
    ELLE_ATTRIBUTE_R(infinit::cryptography::rsa::PublicKey, owner_K);
    infinit::cryptography::Signature _signature;

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
             infinit::cryptography::rsa::PublicKey const& owner_K,
             papier::Authority const& authority);

  public:
    /// Check the passport signature.
    bool
    validate(papier::Authority const&) const;

    /// Dump the passport.
    void
    dump(const elle::Natural32 = 0) const;

    /// Passport is fileable.
    ELLE_CONCEPT_FILEABLE_METHODS();

    void
    print(std::ostream& stream) const;

    bool operator == (Passport const&) const;
    bool operator != (Passport const&) const;

    bool operator < (Passport const&) const;

  private:
    ELLE_SERIALIZE_FRIEND_FOR(Passport);
  };
}

namespace std
{
  template<>
  struct hash<papier::Passport>
  {
  public:
    std::size_t operator()(papier::Passport const& s) const;
  };
}

# include <papier/Passport.hxx>

#endif
