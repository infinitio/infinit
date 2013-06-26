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
    /// A sequence of Location, each of which indicates the address and revision
    /// number of the component. This is basically a Route in term of resolved
    /// data blocks instead of string components.
    class Venue
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef std::vector<nucleus::proton::Location> Container;
      typedef Container::iterator               Iterator;
      typedef Container::const_iterator         Scoutor;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      Venue();
      Venue(Venue const&) = default;
      // XXX: should not be assignable.
      ELLE_OPERATOR_ASSIGNMENT(Venue);
    private:
      ELLE_ATTRIBUTE_RX(Container, elements);

    /*-----------.
    | Operations |
    `-----------*/
    public:
      elle::Status
      Record(nucleus::proton::Location const& location);
      elle::Status
      Record(nucleus::proton::Address const& addr,
             nucleus::proton::Revision const& rev);
      bool
      derives(const Venue&) const;
      elle::Status              Clear();

    /*-----------.
    | Comparable |
    `-----------*/
    public:
      bool
      operator==(const Venue&) const;

    /*---------.
    | Dumpable |
    `---------*/
    public:
      elle::Status              Dump(const elle::Natural32 = 0) const;

    };

  }
}

# include <etoile/path/Venue.hxx>

#endif
