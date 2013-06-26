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
      route(route),
      venue(venue)
    {}

    Chemin::Chemin(Route const& route,
                   Venue const& venue,
                   elle::Size size):
      route(route, size),
      venue(venue, size)
    {}

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method returns true if the current chemin derives the
    /// given base.
    ///
    /// let us imagine two chemins A and B. A is said to derive B
    /// if both its route and venue are included in B, plus potential
    /// other entries. in other words A's route and venue can be equal
    /// or longer.
    ///
    elle::Boolean       Chemin::Derives(const Chemin&            base) const
    {
      if ((this->route.derives(base.route) == true) &&
          (this->venue.derives(base.venue) == true))
        return (true);

      return (false);
    }

    ///
    /// this method generates a Location based on the route and venue.
    ///
    elle::Status        Chemin::Locate(nucleus::proton::Location& location) const
    {
      // check the size of the venue.
      if (this->venue.elements().size() == 0)
        throw Exception("the venue seems to be empty");

      // set the location's attributes according to the venue last element.
      location = this->venue.elements()[this->venue.elements().size() - 1];

      return elle::Status::Ok;
    }

    bool
    Chemin::empty() const
    {
      return this->route == Route::Root && this->venue.elements().empty();
    }

//
// ---------- object ----------------------------------------------------------
//

    ///
    /// this operator compares two objects.
    ///
    elle::Boolean       Chemin::operator==(const Chemin&        element) const
    {
      // check the address as this may actually be the same object.
      if (this == &element)
        return true;

      // compare the attributes.
      if ((this->route != element.route) ||
          (this->venue != element.venue))
        return false;

      return true;
    }

    ///
    /// this operator compares two objects.
    ///
    elle::Boolean       Chemin::operator<(const Chemin&         element) const
    {
      // check the address as this may actually be the same object.
      if (this == &element)
        return false;

      // compare the route only.
      if (this->route < element.route)
        return true;

      return false;
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps a chemin.
    ///
    elle::Status        Chemin::Dump(const elle::Natural32      margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Chemin] " << this << std::endl;

      // dump the route.
      if (this->route.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the route");

      // dump the venue.
      if (this->venue.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the venue");

      return elle::Status::Ok;
    }


    std::ostream&
    operator << (std::ostream& stream, Chemin const& c)
    {
      stream << c.route;
      return stream;
    }

  }
}
