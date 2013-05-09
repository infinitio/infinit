#ifndef ETOILE_GEAR_DIRECTORY_HH
# define ETOILE_GEAR_DIRECTORY_HH

# include <elle/types.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Limits.hh>

# include <etoile/gear/Object.hh>
# include <etoile/gear/Nature.hh>

# include <etoile/nest/fwd.hh>

# include <etoile/automaton/Directory.hh>

# include <etoile/wall/Directory.hh>

namespace etoile
{
  namespace gear
  {

    ///
    /// this class represents a directory-specific context.
    ///
    /// this context derives the Object context and therefore benefits from
    /// all the object-related attributes plus the contents i.e the catalog
    /// in the case of a directory.
    ///
    class Directory:
      public Object
    {
    public:
      //
      // constants
      //
      static const Nature                       N = NatureDirectory;

      //
      // types
      //
      typedef wall::Directory W;
      typedef automaton::Directory A;
      typedef nucleus::neutron::Catalog C;

      //
      // constructors & destructors
      //
      Directory();
      ~Directory();

      //
      // interfaces
      //

      // dumpable
      elle::Status      Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      nucleus::proton::Porcupine<nucleus::neutron::Catalog>* contents_porcupine;
      etoile::nest::Nest* contents_nest;
      nucleus::proton::Limits contents_limits;
      nucleus::proton::Footprint contents_threshold;
    };

  }
}

#endif
