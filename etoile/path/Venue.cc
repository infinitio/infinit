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

    Venue::Venue(Venue const& source, elle::Size size):
      _elements()
    {
      ELLE_ASSERT_LTE(size, source.elements().size());
      unsigned i = 0;
      for (auto const& component: source.elements())
      {
        if (++i > size)
          break;
        this->_elements.push_back(component);
      }
    }

    /*-----------.
    | Operations |
    `-----------*/

    void
    Venue::append(nucleus::proton::Location const& location)
    {
      this->elements().push_back(location);
    }

    void
    Venue::append(nucleus::proton::Address const& address,
                  nucleus::proton::Revision const& revision)
    {
      this->append(nucleus::proton::Location(address, revision));
    }

    // FIXME: factor with route::derives
    bool
    Venue::derives(const Venue& base) const
    {
      if (base.elements().size() > this->elements().size())
        return false;

      auto self = this->elements().begin();
      for (auto const& chunk: base.elements())
        if (*(self++) != chunk)
          return false;
      return true;
    }

    /*-----------.
    | Comparable |
    `-----------*/

    bool
    Venue::operator==(Venue const& other) const
    {
      return this->elements() == other.elements();
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
