#ifndef ETOILE_PATH_ROUTE_HH
# define ETOILE_PATH_ROUTE_HH

# include <elle/types.hh>
# include <elle/operator.hh>

# include <etoile/path/fwd.hh>

# include <vector>

namespace etoile
{
  namespace path
  {
    /// A route is a sequence of slabs forming a path, each slab representing
    /// the name of subdirectory down to the target object along with their
    /// revision numbers.
    ///
    /// Note that this class also contains the revision number of the root
    /// directory. indeed, the first slab is always used for representing the
    /// root directory even though its slab is empty.
    class Route
    {
    public:
      /// The root directory.
      static Route                      Root;

    /*------.
    | Types |
    `------*/
    public:
      typedef std::vector<std::string> Container;
      typedef Container::iterator Iterator;
      typedef Container::const_iterator Scoutor;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Create a Route representing the root.
      Route();
      /// Clone a Route.
      Route(Route const&) = default;
      /// Create a route from a string by splitting it according to the path
      /// separator.
      ///
      /// Note that the first slab is always empty in order to represent the
      /// root directory.  The following assumes the root revision indicator is
      /// '@' while the slab revision indicator is '%'.
      ///
      /// Note that the ways can embed revision numbers as shown next:
      ///
      ///   /suce%42/avale/leche.txt%3
      ///
      /// However, the format '%[0-9]+' cannot be used with the root directory
      /// since the way always starts with '/...'.  In order to provide this
      /// functionality, the following check is made: if the first non-empty
      /// slab starts with '@[0-9]+', then this slab is used as the root one
      /// with the appropriate revision number.
      Route(std::string const& path);
      /// Create a route by appending a component to an existing route.
      Route(const Route& route, const std::string& component);

      elle::Boolean             Derives(const Route&) const;

      elle::Status              Clear();

      //
      // interfaces
      //

      ELLE_OPERATOR_ASSIGNMENT(Route); // XXX

      elle::Boolean             operator==(const Route&) const;
      elle::Boolean             operator<(const Route&) const;

      // dumpable
      elle::Status              Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      Container                 elements;
    };

    std::ostream&
    operator << (std::ostream& stream, Route const& r);
  }
}

# include <etoile/path/Route.hxx>

#endif
