#ifndef ETOILE_ETOILE_HH
# define ETOILE_ETOILE_HH

# include <elle/types.hh>

# include <etoile/gear/Actor.hh>
# include <etoile/gear/Identifier.hh>

namespace etoile
{
  /// Filesystem interface to infinit.
  class Etoile
  {
  public:
    Etoile();
    ~Etoile();

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
