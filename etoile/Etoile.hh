#ifndef ETOILE_ETOILE_HH
# define ETOILE_ETOILE_HH

# include <elle/types.hh>

# include <cryptography/KeyPair.hh>

# include <etoile/depot/Depot.hh>
# include <etoile/gear/Actor.hh>
# include <etoile/gear/Identifier.hh>
# include <etoile/shrub/Shrub.hh>

#include <hole/Hole.hh>

namespace etoile
{
  /// Filesystem interface to infinit.
  class Etoile
  {
  public:
    Etoile(infinit::cryptography::KeyPair const& keys,
           hole::Hole* hole,
           nucleus::proton::Address const& root_address,
           bool portal = true);
    ~Etoile();
  private:
    ELLE_ATTRIBUTE_R(infinit::cryptography::KeyPair, keys);
    ELLE_ATTRIBUTE(bool, portal);

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
