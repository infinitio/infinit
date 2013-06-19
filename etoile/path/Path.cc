#include <boost/lexical_cast.hpp>

#include <reactor/scheduler.hh>

#include <etoile/path/Path.hh>
#include <etoile/path/Venue.hh>
#include <etoile/path/Route.hh>
#include <etoile/path/Chemin.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/wall/Directory.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/shrub/Shrub.hh>
#include <etoile/Exception.hh>

#include <etoile/wall/Path.hh> // XXX

#include <nucleus/proton/Revision.hh>
#include <nucleus/proton/Address.hh>
#include <nucleus/neutron/Entry.hh>

#include <hole/Hole.hh>

#include <Infinit.hh>

namespace etoile
{
  namespace path
  {
    elle::Status
    Path::Initialize()
    {
      if (Route::Initialize() == elle::Status::Error)
        throw Exception("unable to initialize the route");
      return elle::Status::Ok;
    }

    elle::Status
    Path::Clean()
    {
      if (Route::Clean() == elle::Status::Error)
        throw Exception("unable to clean the route");
      return elle::Status::Ok;
    }

    elle::Status
    Path::Resolve(const Route& route,
                  Venue& venue)
    {
      nucleus::proton::Address address;
      nucleus::proton::Revision revision;
      Route::Scoutor    scoutor;

      // first ask the shrub i.e path cache to resolve as much as it can.
      shrub::global_shrub->resolve(route, venue);

      assert(venue.elements.size() <= route.elements.size());

      // if complete, return the address i.e without updating the cache.
      if (route.elements.size() == venue.elements.size())
        return elle::Status::Ok;

      // if the cache did not resolve anything.
      if (venue == Venue::Null)
        {
          Slice         slice;

          // retrieve the root directory's address.
          if (depot::Depot::Origin(address) == elle::Status::Error)
            throw Exception("unable to retrieve the address of the root directory");

          // parse the very first slab i.e the root slab in order
          // to extract the revision number. note that the root slab is
          // always empty.
          if (Path::Parse(route.elements[0],
                          slice, revision) == elle::Status::Error)
            throw Exception("unable to extract the revision number from the root slab");

          // check that the slice is empty, as it should for the root
          // directory.
          if (slice.empty() == false)
            throw Exception("the root slice should always be empty");

          // record the root directory in the venue.
          if (venue.Record(address, revision) == elle::Status::Error)
            throw Exception("unable to record the root directory in the venue");
        }

      // set the address/revision with the address of the last resolved element.
      address = venue.elements[venue.elements.size() - 1].address();
      revision = venue.elements[venue.elements.size() - 1].revision();

      // otherwise, resolve manually by retrieving the directory object.
      for (scoutor = route.elements.begin() + venue.elements.size();
           scoutor != route.elements.end();
           scoutor++)
        {
          Chemin                chemin;
          Slice                 slice;
          nucleus::neutron::Entry const* entry;

          // extract the slice/revision from the current slab.
          if (Path::Parse(*scoutor,
                          slice,
                          revision) == elle::Status::Error)
            throw Exception("unable to extract the slice/revision from the "
                   "current slab");

          // check that the slice is not empty.
          if (slice.empty() == true)
            throw Exception("the slice should never be empty");

          // create the chemin.
          if (chemin.Create(route, venue,
                            venue.elements.size()) == elle::Status::Error)
            throw Exception("unable to create the chemin");

          // load the directory.
          gear::Identifier identifier(wall::Directory::load(chemin));

          // lookup the slice.
          if (wall::Directory::Lookup(identifier,
                                      slice,
                                      entry) == elle::Status::Error)
            {
              // discard the directory.
              wall::Directory::discard(identifier);

              throw Exception("unable to lookup the slice");
            }

          // set the address; the revision is already set i.e it has
          // been extracted from the slab.
          if (entry != nullptr)
            address = entry->address();

          // discard the directory.
          wall::Directory::discard(identifier);

          // if there is no such entry, abort.
          //
          // note that the pointer is used to know whether or not the
          // lookup has succeded. however, the entry's content cannot be
          // accessed as it has potentially been released with the context
          // through the call to Discard().
          if (entry == nullptr)
            throw wall::NoSuchFileOrDirectory(*reactor::Scheduler::scheduler(),
                                              slice);

          // first, record the address/revision in the venue.
          if (venue.Record(address, revision) == elle::Status::Error)
            throw Exception("unable to record the venue address");
        }

      // update the shrub with the resolved path.
      shrub::global_shrub->update(route, venue);

      return elle::Status::Ok;
    }

    elle::Status
    Path::Parse(const std::string& slab,
                Slice& slice,
                nucleus::proton::Revision& revision)
    {
      // set the slice as being the entire slab.
      slice = slab;

      // and set the version as being the latest possible.
      revision = nucleus::proton::Revision::Last;

      /* XXX: handle history
      // if the history mechanism is not supported by the network or
      // has not been activated through the user's configuration, return.
      if ((hole::Hole::Descriptor.meta().history() == false) ||
          (Infinit::Configuration.etoile.history.status == false))
        {
          // set the slice as being the entire slab.
          slice = slab;

          // and set the revision as being the latest possible.
          revision = nucleus::proton::Revision::Last;
        }
      else
        {
          //
          // otherwise, try to handle the history parsing.
          //
          Length                    length;
          size_t                    start;
          nucleus::proton::Revision::Type n;

          // compute the start index, should the in-path revisioning be
          // activated.
          if (Infinit::Configuration.etoile.history.status == true)
            {
              // try to locate the start index for the revision number.
              start = slab.find_last_of(
                        Infinit::Configuration.etoile.history.indicator.slab);
            }
          else
            {
              // set the start index as invalid since the user's
              // configuration has not activated this functionality.
              start = elle::String::npos;
            }

          // if a revision seems to have been found.
          if (start != elle::String::npos)
            {
              // retrieve the slab's length.
              length = slab.length();

              // retrieve the slice.
              slice = elle::String(slab, 0, start);

              try
                {
                  n = boost::lexical_cast<nucleus::proton::Revision::Type>(
                      elle::String(slab, start + 1, length - (start + 1))
                  );
                }
              catch (std::exception const& err)
                {
                  throw Exception("unable to retreive the revision number: %s", err.what());
                }

              if (revision.Create(n) == elle::Status::Error)
                throw Exception("unable to create the revision");
            }
          else
            {
              //
              // otherwise, act as if history was not supported by the
              // network.
              //

              // set the slice as being the entire slab.
              slice = slab;

              // and set the revision as being the latest possible.
              revision = nucleus::proton::Revision::Last;
            }
        }
      */
      return elle::Status::Ok;
    }
  }
}
