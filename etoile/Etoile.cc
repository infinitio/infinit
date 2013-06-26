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
  Etoile::Etoile()
  {
    if (Infinit::Configuration.etoile.shrub.frequency)
    {
      auto capacity = Infinit::Configuration.etoile.shrub.capacity;
      auto lifespan = Infinit::Configuration.etoile.shrub.lifespan;
      auto sweep_frequency = Infinit::Configuration.etoile.shrub.frequency;
      shrub::global_shrub = new shrub::Shrub(
        capacity,
        boost::posix_time::milliseconds(lifespan),
        boost::posix_time::milliseconds(sweep_frequency));
    }

    if (portal::Portal::Initialize() == elle::Status::Error)
      throw Exception("unable to initialize the portal");
  }

  Etoile::~Etoile()
  {
    if (portal::Portal::Clean() == elle::Status::Error)
      throw Exception("unable to clean the portal");

    if (shrub::global_shrub)
    {
      delete shrub::global_shrub;
      shrub::global_shrub = nullptr;
    }

    // Clean Actor
    {
      gear::Actor::Scoutor    scoutor;
      for (auto& actor: gear::Actor::Actors)
        delete actor.second;

      // clear the container.
      gear::Actor::Actors.clear();
    }

    // Clean Scope.
    {
      gear::Scope::S::O::Scoutor    scoutor;
      for (auto scope: gear::Scope::Scopes::Onymous)
        delete scope.second;
      gear::Scope::Scopes::Onymous.clear();
    }
  }

}
