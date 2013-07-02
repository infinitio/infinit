#ifndef ETOILE_WALL_FILE_HH
# define ETOILE_WALL_FILE_HH

# include <elle/types.hh>
# include <elle/fwd.hh>

# include <nucleus/neutron/fwd.hh>

# include <etoile/path/fwd.hh>
# include <etoile/gear/fwd.hh>

namespace etoile
{
  namespace wall
  {
    ///
    /// this class provides functionalities for managing file objects.
    ///
    class File
    {
      /*---------------.
      | Static Methods |
      `---------------*/
    public:
      /// Create a new file object.
      ///
      /// Note however that the object is not attached to the hierarchy
      /// and is therefore considered as orphan.
      static
      gear::Identifier
      create(etoile::Etoile& etoile);
      /// Load the file and returns an identifier for manipuling it.
      static
      gear::Identifier
      load(etoile::Etoile& etoile, path::Chemin const& chemin);
      /// Write the file with the given region of data.
      static
      void
      write(etoile::Etoile& etoile,
            gear::Identifier const& identifier,
            nucleus::neutron::Offset const& offset,
            elle::ConstWeakBuffer data);
      /// Read _size_ bytes of data from the file, at the given offset
      /// _offset_.
      static
      elle::Buffer
      read(etoile::Etoile& etoile,
           gear::Identifier const& idenifier,
           nucleus::neutron::Offset const& offset,
           nucleus::neutron::Size const& size);

      static
      void
      adjust(etoile::Etoile& etoile,
             const gear::Identifier&,
             const nucleus::neutron::Size&);

      /// Discard the scope, potentially ignoring some modifications.
      static
      void
      discard(etoile::Etoile& etoile,
              gear::Identifier const& identifier);
      /// Commit the pending modifications by placing the scope in the journal.
      static
      void
      store(etoile::Etoile& etoile,
            gear::Identifier const& identifier);

      static
      void
      destroy(etoile::Etoile& etoile,
              const gear::Identifier&);
    };
  }
}

#endif
