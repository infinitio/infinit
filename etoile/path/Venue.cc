#include <etoile/path/Venue.hh>
#include <etoile/Exception.hh>

namespace etoile
{
  namespace path
  {
    /*-------------.
    | Construction |
    `-------------*/

    Venue::Venue()
    {}

    elle::Status
    Venue::Record(nucleus::proton::Location const& location)
    {
      this->elements().push_back(location);

      return elle::Status::Ok;
    }

    elle::Status
    Venue::Record(nucleus::proton::Address const& address,
                  nucleus::proton::Revision const& revision)
    {
      nucleus::proton::Location location(address, revision);

      if (this->Record(location) == elle::Status::Error)
        throw Exception("unable to record the location");

      return elle::Status::Ok;
    }

    bool
    Venue::Derives(const Venue& base) const
    {
      auto              i = base.elements().begin();
      auto              j = this->elements().begin();
      auto              end = base.elements().end();

      if (base.elements().size() > this->elements().size())
        return (false);

      for(; i != end; ++i, ++j)
        {
          if (*i != *j)
            return (false);
        }

      return (true);
    }

    elle::Status
    Venue::Clear()
    {
      this->elements().clear();
      return elle::Status::Ok;
    }

    /*-----------.
    | Comparable |
    `-----------*/

    bool
    Venue::operator==(Venue const& element) const
    {
      Venue::Scoutor    s;
      Venue::Scoutor    t;

      // check the address as this may actually be the same object.
      if (this == &element)
        return true;

      // compare the size.
      if (this->elements().size() != element.elements().size())
        return false;

      // for every element.
      for (s = this->elements().begin(), t = element.elements().begin();
           s != this->elements().end();
           s++, t++)
        if (*s != *t)
          return false;

      return true;
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status
    Venue::Dump(elle::Natural32 margin) const
    {
      elle::String alignment(margin, ' ');
      Venue::Scoutor scoutor;

      std::cout << alignment << "[Venue] " << this
                << " #" << std::dec
                << this->elements().size() << std::endl;

      for (scoutor = this->elements().begin();
           scoutor != this->elements().end();
           scoutor++)
      {
        if (scoutor->Dump(margin + 2) == elle::Status::Error)
          throw Exception("unable to dump the address");
      }

      return elle::Status::Ok;
    }
  }
}
