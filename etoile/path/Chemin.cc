#include <etoile/path/Chemin.hh>
#include <etoile/Exception.hh>

namespace etoile
{
  namespace path
  {
    /*-------------.
    | Construction |
    `-------------*/

    Chemin::Chemin()
    {}

    Chemin::Chemin(Route const& route,
                   Venue const& venue):
      _route(route),
      _venue(venue)
    {}

    Chemin::Chemin(Route const& route,
                   Venue const& venue,
                   elle::Size size):
      _route(route, size),
      _venue(venue, size)
    {}

    /*-----------.
    | Operations |
    `-----------*/

    bool
    Chemin::derives(const Chemin& base) const
    {
      return
        this->_route.derives(base._route) && this->_venue.derives(base._venue);
    }

    nucleus::proton::Location
    Chemin::locate() const
    {
      if (this->_venue.elements().size() == 0)
        throw Exception("the venue seems to be empty");
      return this->_venue.elements()[this->_venue.elements().size() - 1];
    }

    bool
    Chemin::empty() const
    {
      return this->_route == Route::Root && this->_venue.elements().empty();
    }

    /*----------.
    | Orderable |
    `----------*/

    bool
    Chemin::operator==(const Chemin& other) const
    {
      if (this == &other)
        return true;
      return this->_route == other._route && this->_venue == other._venue;
    }

    bool
    Chemin::operator<(const Chemin& other) const
    {
      if (this == &other)
        return false;
      // Compare the route only.
      if (this->_route < other._route)
        return true;
      return false;
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status
    Chemin::Dump(const elle::Natural32 margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Chemin] " << this << std::endl;
      if (this->_route.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the route");
      if (this->_venue.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the venue");
      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    std::ostream&
    operator << (std::ostream& stream, Chemin const& c)
    {
      stream << c.route();
      return stream;
    }

  }
}
