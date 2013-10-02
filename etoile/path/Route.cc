#include <elle/system/system.hh>
#include <elle/assert.hh>

#include <etoile/path/Path.hh>
#include <etoile/path/Route.hh>
#include <etoile/Exception.hh>

namespace etoile
{
  namespace path
  {

    Route Route::Root(std::string(1, elle::system::path::separator));

    /*-------------.
    | Construction |
    `-------------*/

    Route::Route():
      Route(std::string(1, elle::system::path::separator))
    {}

    Route::Route(Route const& source,
                 elle::Size size):
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

    Route::Route(std::string const& path):
      _elements()
    {
      elle::String::size_type start;
      elle::String::size_type end;
      std::string slab;

      // check that the path starts with a leading '/'
      if (path[0] != elle::system::path::separator)
        throw Exception
          (elle::sprintf("the path must contain "
                         "the leading path separator '%c': %s",
                         elle::system::path::separator, path));

      // compute the next offsets.
      start =
        path.find_first_not_of(elle::system::path::separator);
      end =
        path.find_first_of(elle::system::path::separator, start);

      // check if at least one slab is present.
      if (start < path.length())
      {
        // create the slab.
        slab = path.substr(start, end - start);

        // XXX: restore history handling
        // // check if the slab represents the root directory i.e starts
        // // with '@' and follows with a possible revision number, should
        // // the network support history though.
        // if ((hole::Hole::instance().descriptor().meta().history() == true) &&
        //     (Infinit::Configuration.etoile.history.status == true) &&
        //     (slab[0] ==
        //      Infinit::Configuration.etoile.history.indicator.root))
        //   {
        //     // modify the '@' character with the revision indicator '%'.
        //     slab[0] = Infinit::Configuration.etoile.history.indicator.slab;

        //     // record the slab.
        //     this->elements.push_back(slab);

        //     // compute the next offsets.
        //     start =
        //       path.find_first_not_of(elle::system::path::separator, end);
        //     end =
        //       path.find_first_of(elle::system::path::separator, start);
        //   }
      }

      // if no slab is present or the first slab does not represent the
      // root directory---i.e the elements container is empty---register
      // an empty root slab.
      if (this->elements().empty() == true)
      {
        // record an empty root slab.
        this->elements().push_back("");
      }

      // then, go through the string.
      while (start < path.length())
      {
        // create the slab.
        slab = path.substr(start, end - start);

        // add the section to the container.
        this->elements().push_back(slab);

        // compute the next offsets.
        start =
          path.find_first_not_of(elle::system::path::separator, end);
        end =
          path.find_first_of(elle::system::path::separator, start);
      }
    }

    Route::Route(const Route& route,
                 const std::string& slab)
    {
      this->elements() = route.elements();
      this->elements().push_back(slab);
    }

    /*-----------.
    | Operations |
    `-----------*/

    bool
    Route::derives(Route const& base) const
    {
      if (base.elements().size() > this->elements().size())
        return false;

      auto self = this->elements().begin();
      for (auto const& chunk: base.elements())
        if (*(self++) != chunk)
          return false;
      return true;
    }

    /*----------.
    | Orderable |
    `----------*/

    bool
    Route::operator==(Route const& other) const
    {
      return this->elements() == other.elements();
    }

    bool
    Route::operator<(Route const& other) const
    {
      return this->elements() < other.elements();
    }

    /*---------.
    | Dumpable |
    `---------*/

    elle::Status
    Route::Dump(const elle::Natural32 margin) const
    {
      elle::String      alignment(margin, ' ');
      Route::Scoutor    scoutor;

      std::cout << alignment << "[Route] " << this
                << " #" << std::dec
                << this->elements().size() << std::endl;

      for (auto const& chunk: this->elements())
        std::cout << alignment << elle::io::Dumpable::Shift
                  << chunk << std::endl;

      return elle::Status::Ok;
    }

    /*----------.
    | Printable |
    `----------*/

    std::ostream&
    operator << (std::ostream& stream,
                 Route const& r)
    {
      for (auto elt: r.elements())
        stream << elt << "/";
      return stream;
    }
  }
}
