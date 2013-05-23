#include <etoile/Etoile.hh>
#include <etoile/path/Path.hh>
#include <etoile/gear/Gear.hh>
#include <etoile/shrub/Shrub.hh>
#include <etoile/portal/Portal.hh>
#include <etoile/Exception.hh>

#include <Infinit.hh>

namespace etoile
{

//
// ---------- methods ---------------------------------------------------------
//

  ///
  /// this method initializes Etoile.
  ///
  elle::Status          Etoile::Initialize()
  {
    if (path::Path::Initialize() == elle::Status::Error)
      throw Exception("unable to initialize the path");

    if (gear::Gear::Initialize() == elle::Status::Error)
      throw Exception("unable to initialize the gear");

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

    return elle::Status::Ok;
  }

  ///
  /// this method cleans Etoile.
  ///
  elle::Status          Etoile::Clean()
  {
    if (portal::Portal::Clean() == elle::Status::Error)
      throw Exception("unable to clean the portal");

    if (shrub::global_shrub)
    {
      delete shrub::global_shrub;
      shrub::global_shrub = nullptr;
    }

    if (gear::Gear::Clean() == elle::Status::Error)
      throw Exception("unable to clean the gear");

    if (path::Path::Clean() == elle::Status::Error)
      throw Exception("unable to clean the path");

    return elle::Status::Ok;
  }

}
