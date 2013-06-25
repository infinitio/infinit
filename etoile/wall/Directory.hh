#ifndef ETOILE_WALL_DIRECTORY_HH
# define ETOILE_WALL_DIRECTORY_HH

# include <elle/types.hh>

# include <nucleus/neutron/fwd.hh>

# include <etoile/path/fwd.hh>
# include <etoile/gear/fwd.hh>

namespace etoile
{
  namespace wall
  {

    ///
    /// this class provides an interface for manipulating directories.
    ///
    class Directory
    {
      /*---------------.
      | Static Methods |
      `---------------*/
    public:
      /// Create a directory though orphan since not attached to the hierarchy.
      static
      gear::Identifier
      create();
      /// Load the directory referenced through the given chemin.
      static
      gear::Identifier
      load(path::Chemin const& chemin);
      /// Add an entry to the given directory.
      static
      void
      add(gear::Identifier const& parent,
          std::string const& name,
          gear::Identifier const& child);

      static
      elle::Status
      Lookup(const gear::Identifier&,
             const std::string&,
             nucleus::neutron::Entry const*&);

      /// Return a set of entries located in [index, index + size[.
      static
      nucleus::neutron::Range<nucleus::neutron::Entry>
      consult(gear::Identifier const& identifer,
              nucleus::neutron::Index const& index,
              nucleus::neutron::Size const& size);

      static
      elle::Status
      Rename(const gear::Identifier&,
             const std::string&,
             const std::string&);
      static
      elle::Status
      Remove(const gear::Identifier&,
             const std::string&);

      /// Discard the scope, potentially ignoring some modifications.
      static
      void
      discard(gear::Identifier const& identifier);
      /// Commit the pending modifications by placing the scope in the journal.
      static
      void
      store(gear::Identifier const& identifier);

      static
      elle::Status
      Destroy(const gear::Identifier&);
    };

  }
}

#endif
