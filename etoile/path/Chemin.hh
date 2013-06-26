#ifndef ETOILE_PATH_CHEMIN_HH
# define ETOILE_PATH_CHEMIN_HH

# include <elle/types.hh>
# include <elle/operator.hh>

# include <nucleus/proton/fwd.hh>
# include <nucleus/neutron/Size.hh>

# include <etoile/path/Route.hh>
# include <etoile/path/Venue.hh>

# include <limits>
# include <utility>
ELLE_OPERATOR_RELATIONALS();

namespace etoile
{
  namespace path
  {
    /// this class contains both a logical and physical path referred to
    /// as a chemin i.e path in French.
    ///
    /// note that for debugging purposes, the chemin embeds the route
    /// along with its associated venue.
    ///
    class Chemin
    {
    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// An empty Chemin.
      Chemin();
      /// A Chemin composed of the given \param route and \param venue.
      Chemin(Route const& route,
             Venue const& venue);
      /// A Chemin composed of the given \param route and \param venue limited
      /// to \param size components.
      Chemin(Route const& route,
             Venue const& venue,
             elle::Size size);
      /// A copy of \param source.
      Chemin(Chemin const& source) = default;

      //
      // methods
      //
      elle::Boolean             Derives(const Chemin&) const;

      elle::Status              Locate(nucleus::proton::Location&) const;
      bool
      empty() const;

      //
      // interfaces
      //

      ELLE_OPERATOR_ASSIGNMENT(Chemin); // XXX

      elle::Boolean             operator==(const Chemin&) const;
      elle::Boolean             operator<(const Chemin&) const;

      // dumpable
      elle::Status              Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      Route                     route;
      Venue                     venue;
    };

    std::ostream&
    operator << (std::ostream& stream, Chemin const& c);
  }
}

# include <etoile/path/Chemin.hxx>

#endif
