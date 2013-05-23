#ifndef ETOILE_SHRUB_RIFFLE_HH
# define ETOILE_SHRUB_RIFFLE_HH

# include <elle/types.hh>
# include <elle/utility/Time.hh>

# include <nucleus/proton/Location.hh>

# include <etoile/path/Slab.hh>
# include <etoile/shrub/fwd.hh>

# include <boost/noncopyable.hpp>

# include <map>
# include <utility>

namespace etoile
{
  namespace shrub
  {
    /// A component of the hierarchical file system structure.
    ///
    /// Riffles store the slab and corresponding location for a path's element
    /// but also a pointer to the parent directory. Although it would have been
    /// easier to rely on class inheritance to prevent riffles from storing a
    /// pointer to a useless hierarchy variable, it would have been too much
    /// burden for no benefit.
    class Riffle:
      private boost::noncopyable
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      Riffle(Shrub& owner,
             const path::Slab& slab,
             const nucleus::proton::Location& location,
             Riffle* parent = nullptr);

      ELLE_ATTRIBUTE(Shrub&, shrub);

    public:
      typedef std::pair<path::Slab, Riffle*>    Value;
      typedef std::map<path::Slab, Riffle*>     Container;
      typedef Container::iterator               Iterator;
      typedef Container::const_iterator         Scoutor;

      /// Try resolving a slab within the riffle's scope.
      void
      resolve(const path::Slab&,
              Riffle*&);
      /// Register a child riffle with the given slab and location.
      void
      update(const path::Slab&,
             const nucleus::proton::Location&);
      /// Destroys the child riffle associated with the given slab.
      void
      destroy(const path::Slab&);
      /// Clear the riffle's content.
      void
      flush();
      elle::Status
      Dump(const elle::Natural32 = 0);

      ELLE_ATTRIBUTE_R(path::Slab, slab);
      ELLE_ATTRIBUTE_RW(nucleus::proton::Location, location);
      ELLE_ATTRIBUTE_RX(elle::utility::Time, timestamp);
      ELLE_ATTRIBUTE_R(Riffle*, parent);
      ELLE_ATTRIBUTE_R(Container, children);
    };

  }
}

#endif
