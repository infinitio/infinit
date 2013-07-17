#ifndef ETOILE_WALL_GROUP_HH
# define ETOILE_WALL_GROUP_HH

# include <elle/types.hh>

# include <nucleus/neutron/Fellow.hh>
# include <nucleus/neutron/Group.hh>
# include <nucleus/neutron/Range.hh>
# include <nucleus/neutron/fwd.hh>

# include <etoile/abstract/fwd.hh>
# include <etoile/gear/Identifier.hh>
# include <etoile/gear/fwd.hh>
# include <etoile/Etoile.hh>

namespace etoile
{
  namespace wall
  {

    /// This class provides methods for manipulating groups.
    class Group
    {
    public:
      //
      // static methods
      //
      /// XXX
      static
      std::pair<nucleus::neutron::Group::Identity, gear::Identifier>
      Create(etoile::Etoile& etoile,
             elle::String const& description);
      /// XXX
      static
      gear::Identifier
      Load(etoile::Etoile& etoile,
           typename nucleus::neutron::Group::Identity const& identity);
      /// XXX
      static
      abstract::Group
      Information(etoile::Etoile& etoile,
                  const gear::Identifier& identifier);
      /// XXX
      static
      void
      Add(etoile::Etoile& etoile,
          gear::Identifier const& identifier,
          nucleus::neutron::Subject const& subject);
      /// XXX
      static
      nucleus::neutron::Fellow
      Lookup(etoile::Etoile& etoile,
             gear::Identifier const& identifier,
             nucleus::neutron::Subject const& subject);
      /// XXX
      static
      nucleus::neutron::Range<nucleus::neutron::Fellow>
      Consult(etoile::Etoile& etoile,
              gear::Identifier const& identifer,
              nucleus::neutron::Index const& index,
              nucleus::neutron::Size const& size);
      /// XXX
      static
      void
      Remove(etoile::Etoile& etoile,
             gear::Identifier const& identifier,
             nucleus::neutron::Subject const& subject);
      /// XXX
      static
      void
      Discard(etoile::Etoile& etoile,
              gear::Identifier const& identifier);
      /// XXX
      static
      void
      Store(etoile::Etoile& etoile,
            gear::Identifier const& identifier);
      /// XXX
      static
      void
      Destroy(etoile::Etoile& etoile,
              gear::Identifier const& identifier);
    };

  }
}

#endif
