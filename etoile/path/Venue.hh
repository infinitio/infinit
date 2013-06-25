#ifndef ETOILE_PATH_VENUE_HH
# define ETOILE_PATH_VENUE_HH

# include <elle/types.hh>
# include <elle/operator.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/proton/Location.hh>

# include <vector>

namespace etoile
{
  namespace path
  {

    ///
    /// this class contains the addresses/revisions corresponding to a route.
    ///
    /// a venue is therefore composed of a sequence of Location, each
    /// of which indicates the address and revision number of the component.
    ///
    class Venue
    {
    public:
      //
      // types
      //
      typedef std::vector<nucleus::proton::Location> Container;
      typedef Container::iterator               Iterator;
      typedef Container::const_iterator         Scoutor;

      //
      // constructors & destructors
      //
      Venue();
      Venue(Venue const&) = default;

      //
      // methods
      //
      elle::Status              Record(const nucleus::proton::Location&);
      elle::Status              Record(const nucleus::proton::Address&,
                                       const nucleus::proton::Revision&);

      elle::Boolean             Derives(const Venue&) const;

      elle::Status              Clear();

      //
      // interfaces
      //

      ELLE_OPERATOR_ASSIGNMENT(Venue); // XXX

      elle::Boolean             operator==(const Venue&) const;

      // dumpable
      elle::Status              Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      Container                 elements;
    };

  }
}

# include <etoile/path/Venue.hxx>

#endif
