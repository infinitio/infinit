#ifndef ETOILE_ETOILE_HH
# define ETOILE_ETOILE_HH

# include <elle/types.hh>

# include <cryptography/KeyPair.hh>

# include <etoile/depot/Depot.hh>
# include <etoile/gear/Actor.hh>
# include <etoile/gear/Identifier.hh>
# include <etoile/shrub/Shrub.hh>

# include <nucleus/neutron/Subject.hh>

# include <hole/Hole.hh>

namespace etoile
{
  /// Filesystem interface to infinit.
  class Etoile
  {
    /*-------------.
    | Construction |
    `-------------*/
  public:
    Etoile(infinit::cryptography::KeyPair const& user_keypair,
           hole::Hole* hole,
           nucleus::proton::Address const& root_address);
    ~Etoile();

    /*-----.
    | User |
    `-----*/
  private:
    ELLE_ATTRIBUTE_R(infinit::cryptography::KeyPair, user_keypair);
    ELLE_ATTRIBUTE_R(nucleus::neutron::Subject, user_subject);

    /*-------.
    | Scopes |
    `-------*/
  public:
    std::shared_ptr<gear::Scope>
    scope_acquire(const path::Chemin&);
    std::shared_ptr<gear::Scope>
    scope_supply();
    void
    scope_annihilate(std::shared_ptr<gear::Scope> const&);
    void
    scope_relinquish(std::shared_ptr<gear::Scope> const&);
    void
    scope_update(const path::Chemin&,
                 const path::Chemin&);
    void
    scope_show() const;
    // XXX
    void
    scope_relinquish(gear::Scope const* scope);
  private:
    elle::Boolean
    _scope_exist(const path::Chemin&) const;
    void
    _scope_add(const path::Chemin&,
               std::shared_ptr<gear::Scope>);
    std::shared_ptr<gear::Scope>
    _scope_retrieve(const path::Chemin&) const;
    void
    _scope_remove(const path::Chemin&);
    void
    _scope_add(std::shared_ptr<gear::Scope>);
    void
    _scope_remove(std::shared_ptr<gear::Scope> const&);
    void
    _scope_inclose(std::shared_ptr<gear::Scope>);
    typedef std::map<const path::Chemin,
                     std::shared_ptr<gear::Scope>> OnymousScopes;
    typedef std::list<std::shared_ptr<gear::Scope>> AnonymousScopes;
    ELLE_ATTRIBUTE(OnymousScopes, onymous_scopes);
    ELLE_ATTRIBUTE(AnonymousScopes, anonymous_scopes);

    /*-------.
    | Actors |
    `-------*/
  public:
    gear::Actor*
    actor_get(gear::Identifier const& id) const;
    void
    actor_add(gear::Actor& actor);
    void
    actor_remove(gear::Actor const& actor);
  private:
    typedef std::map<gear::Identifier, gear::Actor*> Actors;
    ELLE_ATTRIBUTE(Actors, actors);

    /*-----.
    | Path |
    `-----*/
  private:
    ELLE_ATTRIBUTE_RX(shrub::Shrub, shrub);

    /*------.
    | Depot |
    `------*/
  private:
    ELLE_ATTRIBUTE_RX(depot::Depot, depot);
    ELLE_ATTRIBUTE_r(nucleus::proton::Network, network);

    /*----------------.
    | Global instance |
    `----------------*/
  public:
    static Etoile* instance();
  private:
    static Etoile* _instance;
  };

}

#endif
