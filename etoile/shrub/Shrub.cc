#include <elle/container/timeline/Bucket.hh>
#include <elle/container/timeline/Timeline.hh>

#include <reactor/scheduler.hh>

#include <etoile/Exception.hh>
#include <etoile/path/Route.hh>
#include <etoile/path/Venue.hh>
#include <etoile/shrub/Shrub.hh>

namespace etoile
{
  namespace shrub
  {
    shrub::Shrub* global_shrub = nullptr;

//
// ---------- definitions -----------------------------------------------------
//

    ///
    /// this container represents the root of the hierarchical data container.
    ///
    Riffle*                             Shrub::Riffles = nullptr;

    ///
    /// this queue maintains the freshness timestamps related to the riffles.
    ///
    /// whenever the capacity is reached, this data structure is used
    /// to quickly locate the least-recently-used riffles.
    ///
    elle::container::timeline::Timeline<Riffle*>             Shrub::Queue;

    /*-------------.
    | Construction |
    `-------------*/

    Shrub::Shrub(elle::Size capacity,
                 boost::posix_time::time_duration const& lifespan,
                 boost::posix_time::time_duration const& sweep_frequency):
      _capacity(capacity),
      _sweep_frequency(sweep_frequency)
    {
      reactor::Scheduler::scheduler()->Every(std::bind(&Shrub::sweeper, this),
                                             "Shrub sweeper", sweep_frequency);
    }

    Shrub::~Shrub()
    {
      // delete the shrub content, if present.
      if (Shrub::Riffles != nullptr)
        {
          // flush the riffle.
          if (Shrub::Riffles->Flush() == elle::Status::Error)
            throw Exception("unable to flush the riffles");

          // release the shrub slot.
          if (Shrub::Queue.Delete(Shrub::Riffles->timestamp,
                                  Shrub::Riffles) == elle::Status::Error)
            throw Exception("unable to remove the riffle");

          // delete the root riffle.
          delete Shrub::Riffles;
          Shrub::Riffles = nullptr;
        }
    }

    void
    Shrub::clear()
    {
      // delete the shrub content, if present.
      if (Shrub::Riffles != nullptr)
        {
          // flush the riffle.
          if (Shrub::Riffles->Flush() == elle::Status::Error)
            throw Exception("unable to flush the riffles");

          // release the shrub slot.
          if (Shrub::Queue.Delete(Shrub::Riffles->timestamp,
                                  Shrub::Riffles) == elle::Status::Error)
            throw Exception("unable to remove the riffle");

          // delete the root riffle.
          delete Shrub::Riffles;
          Shrub::Riffles = nullptr;
        }
    }

    ///
    /// this method allocates _size_ slots for the introduction of
    /// new riffles.
    ///
    void
    Shrub::allocate(const elle::Natural32 size)
    {
      elle::Natural32   i;

      // release as many riffle as requested and possible.
      //
      // note that the _size_ may be larger than the shrub's actual
      // capacity. indeed, a path composed of thousands of components
      // would not fit in a shrub with a capacity of a hundred entries.
      //
      // therefore, the loop is run a limited number of time but stopped
      // as soon as the number of available slots is reached.
      for (i = 0; i < size; i++)
        {
          elle::container::timeline::Bucket<Riffle*>*        bucket;
          Riffle*                       riffle;

          // stop if there are enough available slots to proceed.
          if ((this->_capacity - Shrub::Queue.container.size()) >= size)
            break;

          // stop if the shrub is empty.
          if (Shrub::Queue.container.empty() == true)
            break;

          // retrieve the least-recently-used bucket of riffles.
          bucket = Shrub::Queue.container.begin()->second;

          // retrieve the first bucket's riffle.
          //
          // note that here we do not go through the bucket's riffles because
          // the destruction of one may actually imply the destruction of
          // many others, i.e its children, and therefore perhaps remove
          // the bucket.
          riffle = bucket->container.front();

          // depending on the riffle's parent.
          if (riffle->parent != nullptr)
            {
              // destroy the entry in the parent riffle.
              if (riffle->parent->Destroy(riffle->slab) == elle::Status::Error)
                throw Exception("unable to destroy the riffle");
            }
          else
            {
              //
              // otherwise, the root riffle needs to be released.
              //
              // note that the loop will be exited right after that
              // since the shrub will be empty.
              //

              // flush the riffle.
              if (Shrub::Riffles->Flush() == elle::Status::Error)
                throw Exception("unable to flush the riffles");

              // release the shrub slot.
              if (Shrub::Queue.Delete(Shrub::Riffles->timestamp,
                                      Shrub::Riffles) == elle::Status::Error)
                throw Exception("unable to remove the riffle");

              // delete the root riffle.
              delete Shrub::Riffles;

              // reset the pointer.
              Shrub::Riffles = nullptr;
            }
        }
    }

    ///
    /// this method takes a path in the form of a route and updates
    /// the venue as much as possible through the shrub.
    ///
    void
    Shrub::resolve(path::Route const& route,
                   path::Venue& venue)
    {
      Riffle* riffle;
      path::Route::Scoutor scoutor;
      elle::utility::Time current;
      elle::utility::Time threshold;

      // make sure this root riffle is present.
      if (Shrub::Riffles == nullptr)
        return;

      // resolve the root directory by recording its location.
      if (venue.Record(Shrub::Riffles->location) == elle::Status::Error)
        throw Exception("unable to record the location");

      // retrieve the current timestamp.
      if (current.Current() == elle::Status::Error)
        throw Exception("unable to retrieve the current time");

      // substract the lifespan to the current time rather than adding it
      // to the timestamp of every riffle.
      threshold = current - elle::utility::Duration(
        elle::utility::Duration::UnitMilliseconds,
        this->_lifespan.total_milliseconds());

      // for every element of the route.
      for (riffle = Shrub::Riffles,
             scoutor = route.elements.begin() + 1;
           scoutor != route.elements.end();
           scoutor++)
        {
          // check whether the riffle has expired.
          if (riffle->timestamp < threshold)
            {
              // depending on the riffle's parent.
              if (riffle->parent != nullptr)
                {
                  // destroy this riffle.
                  if (riffle->parent->Destroy(riffle->slab) ==
                      elle::Status::Error)
                    throw Exception("unable to destroy the riffle");
                }
              else
                {
                  //
                  // otherwise, the root riffle needs to be released.
                  //

                  // flush the riffle.
                  if (Shrub::Riffles->Flush() == elle::Status::Error)
                    throw Exception("unable to flush the riffles");

                  // release the shrub slot.
                  if (Shrub::Queue.Delete(Shrub::Riffles->timestamp,
                                          Shrub::Riffles) == elle::Status::Error)
                    throw Exception("unable to remove the riffle");

                  // delete the root riffle.
                  delete Shrub::Riffles;

                  // reset the pointer.
                  Shrub::Riffles = nullptr;
                }

              break;
            }

          //
          // the riffle has not expired. thus try to resolve within it.
          //

          // try to resolve within this riffle.
          if (riffle->Resolve(*scoutor, riffle) == elle::Status::Error)
            throw Exception("unable to resolve the route");

          // check the pointer.
          if (riffle == nullptr)
            break;

          // add the location to the venue.
          if (venue.Record(riffle->location) == elle::Status::Error)
            throw Exception("unable to record the location");
        }
    }

    ///
    /// this method updates the shrub with a route/venue tuple.
    ///
    void
    Shrub::update(const path::Route& route,
                  const path::Venue& venue)
    {
      path::Route::Scoutor r;
      path::Venue::Scoutor v;

      //
      // first, try to resolve the given route in order to know how
      // many slabs are unresolved and will thus be added to the shrub.
      //
      {
        path::Venue             _venue;

        // resolve the route.
        global_shrub->resolve(route, _venue);

        // requests the allocation of enough slots for those elements.
        this->allocate(_venue.elements.size());
      }

      // make sure the root riffle is present, if not create it.
      if (Shrub::Riffles == nullptr)
        {
          // allocate a new root riffle.
          std::unique_ptr<Riffle> riffle(new Riffle);

          // create the riffle.
          if (riffle->Create(route.elements[0],
                             venue.elements[0]) == elle::Status::Error)
            throw Exception("unable to create the riffle");

          // add the riffle to the queue.
          if (Shrub::Queue.Insert(riffle->timestamp,
                                  riffle.get()) == elle::Status::Error)
            throw Exception("unable to add the riffle");

          // set the root riffle.
          Shrub::Riffles = riffle.release();
        }
      else
        {
          // update the root riffle's location.
          Shrub::Riffles->location = venue.elements[0];
        }

      // for every element of the route/venue.
      Riffle* riffle;
      for (riffle = Shrub::Riffles,
             r = route.elements.begin() + 1, v = venue.elements.begin() + 1;
           (r != route.elements.end()) && (v != venue.elements.end());
           r++, v++)
        {
          // update the riffle with the new location.
          if (riffle->Update(*r, *v) == elle::Status::Error)
            throw Exception("unable to update the riffle");

          // try to resolve within this riffle.
          //
          // note that the previous update may have led to no change
          // so that resolving the slab fails.
          if (riffle->Resolve(*r, riffle) == elle::Status::Error)
            throw Exception("unable to resolve the route");

          // check the pointer.
          if (riffle == nullptr)
            break;
        }
    }

    ///
    /// this method takes a route and removes it from the shrub.
    ///
    void
    Shrub::evict(const path::Route& route)
    {
      Riffle* riffle;
      path::Route::Scoutor scoutor;

      // make sure the root riffle is present.
      if (global_shrub->Riffles == nullptr)
        return;

      // for every element of the route/venue.
      for (riffle = Shrub::Riffles,
             scoutor = route.elements.begin() + 1;
           scoutor < route.elements.end();
           scoutor++)
        {
          // try to resolve within this riffle.
          //
          // if this process fails, it would mean that the given route
          // is not held in the shrub.
          if (riffle->Resolve(*scoutor, riffle) == elle::Status::Error)
            throw Exception("unable to resolve the route");

          // check the pointer.
          if (riffle == nullptr)
            return;
        }

      //
      // at this point, _riffle_ points to the riffle associated with the
      // given route.
      //

      // destroy the riffle by removing it from its parent. should this parent
      // not exist---i.e for the root riffle---reset the root pointer.
      if (riffle->parent != nullptr)
        {
          // destroy the entry in the parent riffle.
          if (riffle->parent->Destroy(riffle->slab) == elle::Status::Error)
            throw Exception("unable to destroy the riffle's entry");
        }
      else
        {
          //
          // otherwise, the route references the root riffle.
          //

          // flush the riffle.
          if (Shrub::Riffles->Flush() == elle::Status::Error)
            throw Exception("unable to flush the riffles");

          // release the shrub slot.
          if (Shrub::Queue.Delete(Shrub::Riffles->timestamp,
                                  Shrub::Riffles) == elle::Status::Error)
            throw Exception("unable to remove the riffle");

          // delete the root riffle.
          delete Shrub::Riffles;

          // reset the pointer.
          Shrub::Riffles = nullptr;
        }
    }

    ///
    /// this method dumps the whole shrub.
    ///
    void
    Shrub::show(const elle::Natural32 margin)
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Shrub]" << std::endl;

      // make sure this root riffle is present.
      if (Shrub::Riffles != nullptr)
        {
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Riffles]" << std::endl;

          // just initiate a recursive dump, starting with the root riffle.
          if (Shrub::Riffles->Dump(margin + 4) == elle::Status::Error)
            throw Exception("unable to dump the shrub's riffles");
        }

      // dump the queue.
      if (Shrub::Queue.Dump(margin + 4) == elle::Status::Error)
        throw Exception("unable to dump the queue");
    }

//
// ---------- static callbacks ------------------------------------------------
//

    ///
    /// this callback is triggered on a periodic basis in order to evict
    /// the expired riffle.
    ///
    /// note that most riffles are removed during the resolving process
    /// when detecting they have expired. however, riffles being never
    /// accessed must be removed by using the sweeper.
    ///
    void
    Shrub::sweeper()
    {
      elle::utility::Time        current;
      elle::utility::Time        threshold;

      // retrieve the current timestamp.
      if (current.Current() == elle::Status::Error)
        throw Exception("unable to retrieve the current time");

      // substract the lifespan to the current time rather than adding it
      // to the timestamp of every riffle.
      threshold = current - elle::utility::Duration(
        elle::utility::Duration::UnitMilliseconds,
        this->_lifespan.total_milliseconds());

      // go through the queue as long as the riffles have expired i.e
      // their update timestamp has reached the threshold.
      while (Shrub::Queue.container.empty() == false)
        {
          elle::container::timeline::Bucket<Riffle*>*        bucket;
          Riffle*                       riffle;

          // retrieve the least-recently-used bucket.
          bucket = Shrub::Queue.container.begin()->second;

          // retrieve the first bucket's riffle.
          //
          // note that here we do not go through the bucket's riffles because
          // the destruction of one may actually imply the destruction of
          // many others, i.e its children, and therefore perhaps remove
          // the bucket.
          riffle = bucket->container.front();

          // if the riffle has not expired, exit the loop since all the
          // following riffles are fresher.
          if (riffle->timestamp >= threshold)
            break;

          // depending on the riffle's parent.
          if (riffle->parent != nullptr)
            {
              // destroy this riffle.
              if (riffle->parent->Destroy(riffle->slab) == elle::Status::Error)
                throw Exception("unable to destroy the riffle");
            }
          else
            {
              //
              // otherwise, the root riffle needs to be released.
              //

              // flush the riffle.
              if (Shrub::Riffles->Flush() == elle::Status::Error)
                throw Exception("unable to flush the riffles");

              // release the shrub slot.
              if (Shrub::Queue.Delete(Shrub::Riffles->timestamp,
                                      Shrub::Riffles) == elle::Status::Error)
                throw Exception("unable to remove the riffle");

              // delete the root riffle.
              delete Shrub::Riffles;

              // reset the pointer.
              Shrub::Riffles = nullptr;
            }
        }
    }

  }
}
