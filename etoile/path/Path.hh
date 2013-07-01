#ifndef ETOILE_PATH_PATH_HH
# define ETOILE_PATH_PATH_HH

#include <elle/types.hh>
#include <nucleus/proton/fwd.hh>

#include <etoile/Etoile.hh>
#include <etoile/path/fwd.hh>

namespace etoile
{
  /// Everything necessary for resolving paths.
  namespace path
  {
    /// Entry point to path resolution.
    class Path
    {
    public:
      /// Take a route and returns the address of the referenced object.
      ///
      /// Start by resolving the route by looking up in the shrub, retreive the
      /// uncached directory objects and explore them.  Note that this method
      /// only processes absolute paths. Paths being composed of links will fail
      /// to be resolved for instance.
      static
      Venue
      Resolve(etoile::Etoile& etoile,
              const Route& route);

      /// Take a slice and tries to extract both the real slice and the revision
      /// number.
      ///
      /// For instance the slice 'teton.txt%42' - assuming the regexp '%[0-9]+'
      /// is used for revision numbers - would be split into 'teton.txt' and the
      /// revision number 42.
      static
      elle::Status
      Parse(const std::string& slab,
            std::string& slice,
            nucleus::proton::Revision& revision);
    };
  }
}

#endif
