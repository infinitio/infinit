#include <etoile/path/Venue.hh>
#include <etoile/Exception.hh>

namespace etoile
{
  namespace path
  {
//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor.
    ///
    Venue::Venue()
    {
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method records the location.
    ///
    elle::Status        Venue::Record(const nucleus::proton::Location& location)
    {
      // store the location in the container.
      this->elements.push_back(location);

      return elle::Status::Ok;
    }

    ///
    /// this method records the next step of the venue.
    ///
    elle::Status        Venue::Record(const nucleus::proton::Address& address,
                                      const nucleus::proton::Revision& revision)
    {
      nucleus::proton::Location location(address, revision);

      // record the location.
      if (this->Record(location) == elle::Status::Error)
        throw Exception("unable to record the location");

      return elle::Status::Ok;
    }

    ///
    /// this method returns true if the current venue derives the
    /// given base i.e the venue's elements also appears in the base.
    ///
    elle::Boolean       Venue::Derives(const Venue&             base) const
    {
      auto              i = base.elements.begin();
      auto              j = this->elements.begin();
      auto              end = base.elements.end();

      if (base.elements.size() > this->elements.size())
        return (false);

      for(; i != end; ++i, ++j)
        {
          if (*i != *j)
            return (false);
        }

      return (true);
    }

    ///
    /// this method clears the venue's content.
    ///
    elle::Status        Venue::Clear()
    {
      // clear the container.
      this->elements.clear();

      return elle::Status::Ok;
    }

//
// ---------- object ----------------------------------------------------------
//

    ///
    /// this operator compares two objects.
    ///
    elle::Boolean       Venue::operator==(const Venue&          element) const
    {
      Venue::Scoutor    s;
      Venue::Scoutor    t;

      // check the address as this may actually be the same object.
      if (this == &element)
        return true;

      // compare the size.
      if (this->elements.size() != element.elements.size())
        return false;

      // for every element.
      for (s = this->elements.begin(), t = element.elements.begin();
           s != this->elements.end();
           s++, t++)
        if (*s != *t)
          return false;

      return true;
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps a venue.
    ///
    elle::Status        Venue::Dump(const elle::Natural32       margin) const
    {
      elle::String      alignment(margin, ' ');
      Venue::Scoutor    scoutor;

      std::cout << alignment << "[Venue] " << this
                << " #" << std::dec
                << this->elements.size() << std::endl;

      // for every element.
      for (scoutor = this->elements.begin();
           scoutor != this->elements.end();
           scoutor++)
        {
          // dump the location.
          if (scoutor->Dump(margin + 2) == elle::Status::Error)
            throw Exception("unable to dump the address");
        }

      return elle::Status::Ok;
    }

  }
}
