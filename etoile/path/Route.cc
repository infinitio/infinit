#include <elle/system/system.hh>

#include <etoile/path/Path.hh>
#include <etoile/path/Route.hh>
#include <etoile/Exception.hh>

#include <agent/Agent.hh>
#include <hole/Hole.hh>

namespace etoile
{
  namespace path
  {

//
// ---------- definitions -----------------------------------------------------
//

    const Route Route::Null;
    Route Route::Root(std::string(1, elle::system::path::separator));

    /*-------------.
    | Construction |
    `-------------*/

    Route::Route():
      Route(std::string(1, elle::system::path::separator))
    {}

    Route::Route(std::string const& path)
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

      // clear the elements.
      this->elements.clear();

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
      if (this->elements.empty() == true)
      {
        // record an empty root slab.
        this->elements.push_back("");
      }

      // then, go through the string.
      while (start < path.length())
      {
        // create the slab.
        slab = path.substr(start, end - start);

        // add the section to the container.
        this->elements.push_back(slab);

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
      this->elements = route.elements;
      this->elements.push_back(slab);
    }

    ///
    /// this method returns true if the current route derives the
    /// given base i.e the base's elements appear in the route.
    ///
    elle::Boolean       Route::Derives(const Route&             base) const
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
    /// this method clears the route's content.
    ///
    elle::Status        Route::Clear()
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
    elle::Boolean       Route::operator==(const Route&          element) const
    {
      Route::Scoutor    s;
      Route::Scoutor    t;

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

    ///
    /// this operator compares two objects.
    ///
    elle::Boolean       Route::operator<(const Route&           element) const
    {
      Route::Scoutor    s;
      Route::Scoutor    t;

      // check the address as this may actually be the same object.
      if (this == &element)
        return false;

      // compare the size.
      if (this->elements.size() < element.elements.size())
        return true;
      else if (this->elements.size() > element.elements.size())
        return false;

      // for every element.
      for (s = this->elements.begin(), t = element.elements.begin();
           s != this->elements.end();
           s++, t++)
        {
          if (*s < *t)
            return true;
          else if (*s > *t)
            return false;
        }

      // at this point, both routes seem identical.
      return false;
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps a route.
    ///
    elle::Status        Route::Dump(const elle::Natural32       margin) const
    {
      elle::String      alignment(margin, ' ');
      Route::Scoutor    scoutor;

      std::cout << alignment << "[Route] " << this
                << " #" << std::dec
                << this->elements.size() << std::endl;

      // for every element.
      for (scoutor = this->elements.begin();
           scoutor != this->elements.end();
           scoutor++)
        {
          // dump the slab.
          std::cout << alignment << elle::io::Dumpable::Shift
                    << *scoutor << std::endl;
        }

      return elle::Status::Ok;
    }

    std::ostream&
    operator << (std::ostream& stream, Route const& r)
    {
      for (auto elt: r.elements)
        stream << elt << "/";
      return stream;
    }
  }
}
