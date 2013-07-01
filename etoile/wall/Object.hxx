#ifndef  ETOILE_WALL_OBJECT_HXX
# define ETOILE_WALL_OBJECT_HXX

# include <etoile/Etoile.hh>
# include <etoile/Exception.hh>
# include <etoile/gear/Scope.hh>
# include <etoile/path/Chemin.hh>
# include <etoile/path/Path.hh>
# include <etoile/shrub/Shrub.hh>

namespace etoile
{
  namespace wall
  {

      template <typename T>
      void
      Object::reload(gear::Scope& scope)
      {
        ELLE_LOG_COMPONENT("etoile.wall.Object");
        ELLE_TRACE_FUNCTION(scope);

        ELLE_TRACE("clearing the cache in order to evict %s",
                   scope.chemin.route());

        Etoile::instance()->shrub().clear();

        ELLE_TRACE("try to resolve the route now that the cache was cleaned");

        path::Venue venue =
          path::Path::Resolve(*Etoile::instance(), scope.chemin.route());
        scope.chemin = path::Chemin(scope.chemin.route(), venue);

        ELLE_DEBUG("route was successfully resolved into %s",
                   scope.chemin.route());

        ELLE_TRACE("loading object");

        T* context = nullptr;
        if (scope.Use(context) == elle::Status::Error)
          throw Exception("unable to use the context");

        // Reset location
        nucleus::proton::Location location = scope.chemin.locate();

        context->location = location;
        // Force the loading.
        context->object.reset();
        context->state = gear::Context::StateUnknown;

        if (T::A::Load(*context) == elle::Status::Error)
          throw Exception("unable to load the object");
      }

  }
}

#endif
