#include <etoile/shrub/Riffle.hh>
#include <etoile/shrub/Shrub.hh>
#include <etoile/Exception.hh>

#include <Infinit.hh>

namespace etoile
{
  namespace shrub
  {
    /*-------------.
    | Construction |
    `-------------*/

    Riffle::Riffle(Shrub& owner,
                   const std::string& slab,
                   const nucleus::proton::Location& location,
                   Riffle* parent):
      _shrub(owner),
      _slab(slab),
      _location(location),
      _timestamp(),
      _parent(parent),
      _children()
    {
      if (this->_timestamp.Current() == elle::Status::Error)
        throw Exception("unable to retrieve the current time");
    }

    void
    Riffle::resolve(const std::string& slab,
                    Riffle*& riffle)
    {
      Riffle::Scoutor scoutor;
      riffle = nullptr;
      if ((scoutor = this->_children.find(slab)) == this->_children.end())
        return;
      riffle = scoutor->second;
    }

    void
    Riffle::update(const std::string& slab,
                   const nucleus::proton::Location& location)
    {
      auto it = this->_children.find(slab);

      // Try to look up the element in the current riffle.
      if (it == this->_children.end())
      {
        // Check that available slots remain i.e it is possible that
        // the whole shrub's capacity is not large enough to hold
        // all the the route's slabs.
        if (_shrub._queue.container.size() >= _shrub.capacity())
          return;
        std::unique_ptr<Riffle> riffle(new Riffle(this->_shrub,
                                                  slab, location, this));
        if (this->_shrub._queue.Insert(riffle->timestamp(),
                                       riffle.get()) == elle::Status::Error)
          throw Exception("unable to add the riffle");
        auto result = this->_children.insert(
          Riffle::Value(riffle->slab(), riffle.get()));
        // XXX if riffle is in the Shrub::Queue but previous insert failed ...
        if (result.second == false)
          throw Exception("unable to insert the new riffle");
        else
          riffle.release();
      }
      else
      {
        Riffle* riffle = it->second;
        riffle->location(location);
        if (this->_shrub._queue.Delete(riffle->timestamp(),
                                       riffle) == elle::Status::Error)
          throw Exception("unable to remove the riffle");
        if (riffle->timestamp().Current() == elle::Status::Error)
          throw Exception("unable to retrieve the current time");
        if (this->_shrub._queue.Insert(riffle->timestamp(),
                                       riffle) == elle::Status::Error)
          throw Exception("unable to add the riffle");
      }
    }

    void
    Riffle::destroy(const std::string& slab)
    {
      Riffle::Iterator iterator;
      if ((iterator = this->_children.find(slab)) == this->_children.end())
        throw Exception("unable to locate the given slab to destroy");
      Riffle* riffle = iterator->second;
      riffle->flush();
      if (this->_shrub._queue.Delete(riffle->timestamp(),
                                     riffle) == elle::Status::Error)
        throw Exception("unable to remove the riffle");
      delete riffle;
      this->_children.erase(iterator);
    }

    void
    Riffle::flush()
    {
      Riffle::Scoutor   scoutor;

      for (scoutor = this->_children.begin();
           scoutor != this->_children.end();
           scoutor++)
      {
        Riffle*       riffle = scoutor->second;
        riffle->flush();
        if (this->_shrub._queue.Delete(riffle->timestamp(),
                                       riffle) == elle::Status::Error)
          throw Exception("unable to remove the riffle");
        delete riffle;
      }
      this->_children.clear();
    }

    elle::Status
    Riffle::Dump(const elle::Natural32 margin)
    {
      elle::String      alignment(margin, ' ');
      Riffle::Scoutor   scoutor;
      std::cout << alignment << "[Riffle] "
                << std::hex << this << std::endl;
      std::cout << alignment << elle::io::Dumpable::Shift << "[Slab] "
                << this->slab() << std::endl;
      if (this->location().Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the location");
      if (this->timestamp().Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the timestamp");
      std::cout << alignment << elle::io::Dumpable::Shift << "[Parent] "
                << std::hex << this->parent() << std::endl;
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Children]" << std::endl;
      for (scoutor = this->_children.begin();
           scoutor != this->_children.end();
           scoutor++)
      {
        if (scoutor->second->Dump(margin + 4) == elle::Status::Error)
          throw Exception("unable to dump the sub-riffle");
      }
      return elle::Status::Ok;
    }

  }
}
