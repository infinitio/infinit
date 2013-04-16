#include <etoile/automaton/Object.hh>
#include <etoile/automaton/Access.hh>
#include <etoile/automaton/Attributes.hh>
#include <etoile/gear/Object.hh>
#include <etoile/abstract/Object.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/Exception.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/State.hh>
#include <nucleus/neutron/Access.hh>

#include <agent/Agent.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.etoile.automaton.Object");

namespace etoile
{
  namespace automaton
  {

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method fetches the object and opens the access block, should
    /// this block be present.
    ///
    elle::Status        Object::Load(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      // return if the context has already been loaded.
      if (context.state != gear::Context::StateUnknown)
        {
          ELLE_DEBUG("the object's context is already loaded");
          return elle::Status::Ok;
        }

      // XXX[the context should make use of unique_ptr instead
      //     of releasing here.]

      ELLE_TRACE("pull the object from depot at address '%s' and "
                 "revision '%s'",
                 context.location.address(),
                 context.location.revision());

      ELLE_ASSERT(context.object == nullptr);

      context.object.reset(
        depot::Depot::pull_object(
          context.location.address(),
          context.location.revision()).release());

      // Compute the block base.
      context.object->base(nucleus::proton::Base(*context.object));

      // set the context's state.
      context.state = gear::Context::StateLoaded;

      return elle::Status::Ok;
    }

    ///
    /// this method generates an Abstract object which summarises the
    /// object's meta data.
    ///
    elle::Status        Object::Information(
                          gear::Object&                         context,
                          abstract::Object&              abstract)
    {
      ELLE_TRACE_FUNCTION(context);

      // generate the abstract based on the object.
      if (abstract.Create(*context.object) == elle::Status::Error)
        throw Exception("unable to generate the abstract");

      return elle::Status::Ok;
    }

    ///
    /// this method is called whenever the context is being closed without
    /// any modification having been performed.
    ///
    elle::Status        Object::Discard(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      // set the context's state.
      context.state = gear::Context::StateDiscarded;

      return elle::Status::Ok;
    }

    ///
    /// this method permanently destroys the object along with all its
    /// history i.e all its revisions.
    ///
    /// therefore, all the blocks are marked as needing to be removed.
    ///
    /// note however that although the past revisions of the objects will
    /// be destroyed along the way, the referenced access and data blocks
    /// will not.
    ///
    /// although the destroy functionality may suffice for most cases i.e
    /// for objects with a short history or with a small amount of data,
    /// in other cases, the amount of storage capacity lost---until the
    /// blocks expire---may be too important.
    ///
    /// therefore, the user willing to completely remove the blocks
    /// associated with an object, no matter the revision, should take a
    /// look at the Purge() functionality.
    ///
    elle::Status        Object::Destroy(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      // destroy the access.
      if (Access::Destroy(context) == elle::Status::Error)
        throw Exception("unable to destroy the access");

      // mark the object as needing to be removed.
      context.transcript().record(
        new gear::action::Wipe(context.location.address()));

      // set the context's state.
      context.state = gear::Context::StateDestroyed;

      return elle::Status::Ok;
    }

    ///
    /// this method terminates the automaton by making the whole object
    /// consistent according to the set of modifications having been performed.
    ///
    elle::Status        Object::Store(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      Attributes::close(context);

      // if the object has been modified i.e is dirty.
      if (context.object->state() == nucleus::proton::State::dirty)
        {
          Access::Open(context);

          // XXX[not optimized: we should compute the fingerprint while
          //     Upgrade()ing the access tokens]
          cryptography::Digest fingerprint = Access::fingerprint(context);

          Access::Close(context);

          // seal the object alone with the access block.
          if (context.object->Seal(
                agent::Agent::Identity.pair().k(),
                fingerprint) == elle::Status::Error)
            throw Exception("unable to seal the object");

          // mark the block as needing to be stored.
          context.transcript().record(
            new gear::action::Push(context.location.address(),
                                   std::move(context.object)));
        }
      else
        {
          Access::Close(context);
        }

      // set the context's state.
      context.state = gear::Context::StateStored;

      return elle::Status::Ok;
    }

  }
}
