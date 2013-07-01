#include <etoile/Etoile.hh>
#include <etoile/Exception.hh>
#include <etoile/gear/Actor.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/path/Path.hh>
#include <etoile/shrub/Shrub.hh>

#include <Infinit.hh>

namespace etoile
{
  namespace time = boost::posix_time;
  Etoile::Etoile(infinit::cryptography::KeyPair const& user_keypair,
                 hole::Hole* hole,
                 nucleus::proton::Address const& root_address):
    _user_keypair(user_keypair),
    _user_subject(this->_user_keypair.K()),
    _actors(),
    _shrub(),
    _depot(hole, root_address)
  {
    ELLE_ASSERT(!this->_instance);
    this->_instance = this;
  }

  Etoile::~Etoile()
  {
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
