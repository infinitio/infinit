#ifndef ETOILE_WALL_ACCESS_HH
# define ETOILE_WALL_ACCESS_HH

# include <memory>

# include <elle/types.hh>

# include <nucleus/neutron/fwd.hh>

# include <etoile/gear/fwd.hh>

namespace etoile
{
  namespace wall
  {

    /// Functionalities for manipulating the access control mechanism.
    class Access
    {
    public:
      /// The access record associated with the given subject.
      static
      nucleus::neutron::Record
      lookup(etoile::Etoile& etoile,
             gear::Identifier const&,
             nucleus::neutron::Subject const&);
      /// A subset of the object's access access list.
      ///
      /// Use carefully as a set of pointers to the target records is
      /// returned. should one of the records be destroyed by another
      /// actor's operation, accessing it could make the system crash.
      /// XXX[should not be the case in the future though]
      static
      nucleus::neutron::Range<nucleus::neutron::Record>
      consult(etoile::Etoile& etoile,
              gear::Identifier const&,
              nucleus::neutron::Index const&,
              nucleus::neutron::Size const&);
      static elle::Status       Grant(etoile::Etoile& etoile,
                                      const gear::Identifier&,
                                      const nucleus::neutron::Subject&,
                                      const nucleus::neutron::Permissions&);
      static elle::Status       Revoke(etoile::Etoile& etoile,
                                       const gear::Identifier&,
                                       const nucleus::neutron::Subject&);
    };

  }
}

#endif
