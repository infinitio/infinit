#ifndef ETOILE_GEAR_OBJECT_HH
# define ETOILE_GEAR_OBJECT_HH

# include <elle/types.hh>

# include <cryptography/SecretKey.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Limits.hh>
# include <nucleus/proton/Location.hh>
# include <nucleus/neutron/fwd.hh>
# include <nucleus/neutron/Object.hh>
# include <nucleus/neutron/Permissions.hh>
# include <nucleus/neutron/Record.hh>

# include <etoile/gear/Context.hh>
# include <etoile/gear/Nature.hh>

# include <etoile/nest/fwd.hh>

# include <etoile/automaton/Object.hh>

# include <etoile/wall/Object.hh>

namespace etoile
{
  namespace gear
  {

    ///
    /// this class represents an object context.
    ///
    /// although every object references a Contents block which itself
    /// contains the address of the data blocks, the Contents block is
    /// not included in this context as it depends on the object's genre i.e
    /// file, directory or link.
    ///
    /// however, object contexts include an access block which may be missing
    /// should the object be kept private to the owner.
    ///
    /// finally, note that once necessary, the user's rights on the
    /// object are computed; likewise for the authorship.
    ///
    /// note that a context can only be stored, destroyed or discarded.
    ///
    /// in the first case, the set of interconnected blocks are made
    /// consistent and the internal addresses are re-computed to reference
    /// the other blocks.
    ///
    /// in the second case, since the blocks have not been made consistent, no
    /// matter what modifications have been performed, the original address
    /// plus the internal addresses remain unchanged and can be used to
    /// remove the blocks from the storage layer.
    ///
    /// in the last case, the context is simply dropped, hence ignoring
    /// the potential modifications.
    ///
    class Object:
      public Context
    {
    public:
      //
      // constants
      //
      static const Nature                       N = NatureObject;

      //
      // types
      //
      typedef wall::Object W;
      typedef automaton::Object A;

      typedef nucleus::neutron::Object::Role Role;

      //
      // constructors & destructors
      //
      Object();
      Object(const Nature);
      ~Object();

      //
      // interfaces
      //

      // dumpable
      elle::Status      Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      nucleus::proton::Location location;

      std::unique_ptr<nucleus::neutron::Object> object;

      nucleus::proton::Porcupine<nucleus::neutron::Access>* access_porcupine;
      etoile::nest::Nest* access_nest;
      nucleus::proton::Limits access_limits;
      nucleus::proton::Footprint access_threshold;

      nucleus::proton::Porcupine<nucleus::neutron::Attributes>* attributes_porcupine;
      etoile::nest::Nest* attributes_nest;
      nucleus::proton::Limits attributes_limits;
      nucleus::proton::Footprint attributes_threshold;

      struct
      {
        nucleus::neutron::Object::Role role;
        nucleus::neutron::Permissions permissions;
        cryptography::SecretKey* key;

        // XXX[use a shared_ptr<> instead. cf: automaton::Access::lookup()]
        nucleus::neutron::Record* record;
      } rights;
      nucleus::neutron::Author* author;
    };

  }
}

#endif
