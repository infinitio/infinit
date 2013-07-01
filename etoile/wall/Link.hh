#ifndef ETOILE_WALL_LINK_HH
# define ETOILE_WALL_LINK_HH

# include <elle/types.hh>

# include <etoile/path/fwd.hh>
# include <etoile/gear/fwd.hh>

namespace etoile
{
  namespace wall
  {

    ///
    /// this class provides functionalities for managing links.
    ///
    class Link
    {
      /*---------------.
      | Static Methods |
      `---------------*/
    public:
      /// Create a new link object though not yet attached to the
      /// hiearchy i.e orphan.
      static
      gear::Identifier
      create();

      static elle::Status       Load(const path::Chemin&,
                                     gear::Identifier&);

      /// Bind a target way to the link.
      static
      void
      bind(gear::Identifier const& identifier,
           std::string const& target);
      /// Return the path to which the link is bound.
      static
      std::string
      resolve(gear::Identifier const& identifier);

      static elle::Status       Discard(const gear::Identifier&);

      /// Commit the pending modifications by placing the scope in the journal.
      static
      void
      store(etoile::Etoile& etoile,
            gear::Identifier const& identifier);

      /// Destroy a link.
      static
      void
      destroy(etoile::Etoile& etoile,
              const gear::Identifier&);
    };

  }
}

#endif
