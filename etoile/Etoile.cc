#include <etoile/Etoile.hh>
#include <etoile/Exception.hh>
#include <etoile/gear/Actor.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/path/Path.hh>
#include <etoile/portal/Portal.hh>
#include <etoile/shrub/Shrub.hh>

#include <Infinit.hh>

namespace etoile
{
  namespace time = boost::posix_time;
  Etoile::Etoile(hole::Hole* hole):
    _actors(),
    _shrub(Infinit::Configuration.etoile.shrub.capacity,
           time::milliseconds(Infinit::Configuration.etoile.shrub.lifespan),
           time::milliseconds(Infinit::Configuration.etoile.shrub.frequency)),
    _depot(hole)
  {
    if (portal::Portal::Initialize() == elle::Status::Error)
      throw Exception("unable to initialize the portal");

    ELLE_ASSERT(!this->_instance);
    this->_instance = this;
  }

  Etoile::~Etoile()
  {
    if (portal::Portal::Clean() == elle::Status::Error)
      throw Exception("unable to clean the portal");

    for (auto& actor: this->_actors)
      delete actor.second;
    this->_actors.clear();

    // Clean Scope.
    {
      for (auto scope: gear::Scope::Scopes::Onymous)
        delete scope.second;
      gear::Scope::Scopes::Onymous.clear();
    }

    this->_instance = nullptr;
  }

  /*------.
  | Actor |
  `------*/

  gear::Actor*
  Etoile::actor_get(gear::Identifier const& id) const
  {
    auto it = this->_actors.find(id);
    if (it == this->_actors.end())
      throw elle::Exception(elle::sprintf("unkown actor %s", id));
    return it->second;
  }

  void
  Etoile::actor_add(gear::Actor& actor)
  {
    auto id = actor.identifier;
    if (this->_actors.find(id) != this->_actors.end())
      throw elle::Exception(elle::sprintf("duplicate actor %s", id));
    this->_actors[id] = &actor;
  }

  void
  Etoile::actor_remove(gear::Actor const& actor)
  {
    auto id = actor.identifier;
    auto it = this->_actors.find(id);
    if (it == this->_actors.end())
      throw elle::Exception(elle::sprintf("unkown actor %s", id));
    this->_actors.erase(it);
  }

  /*------.
  | Depot |
  `------*/

  nucleus::proton::Network const&
  Etoile::network() const
  {
    return this->_depot.network();
  }

  /*----------------.
  | Global instance |
  `----------------*/

  Etoile*
  Etoile::instance()
  {
    return Etoile::_instance;
  }

  Etoile* Etoile::_instance(nullptr);
}
