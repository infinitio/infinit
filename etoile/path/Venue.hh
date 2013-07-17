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
      /// An empty venue.
      Venue();
      /// A copy of \param source.
      Venue(Venue const& /*source*/) = default;
      /// A copy of \param source limited to the first \param size components.
      Venue(Venue const& source, elle::Size size);
      // XXX: should not be assignable.
      ELLE_OPERATOR_ASSIGNMENT(Venue);
    private:
      ELLE_ATTRIBUTE_RX(Container, elements);

    /*-----------.
    | Operations |
    `-----------*/
    public:
      /// Append the given location.
      void
      append(nucleus::proton::Location const& location);
      /// Append a location composed of the given address and revision.
      void
      append(nucleus::proton::Address const& addr,
             nucleus::proton::Revision const& rev);
      /// Whether this starts with \param base.
      bool
      derives(const Venue& base) const;

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
      elle::Status
      Dump(const elle::Natural32 = 0) const;
    };
  }
}

# include <etoile/path/Venue.hxx>

#endif
