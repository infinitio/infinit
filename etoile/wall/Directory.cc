#include <etoile/wall/Directory.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/Directory.hh>
#include <etoile/gear/Operation.hh>
#include <etoile/gear/Guard.hh>
#include <etoile/automaton/Directory.hh>
#include <etoile/automaton/Rights.hh>
#include <etoile/journal/Journal.hh>
#include <etoile/path/Path.hh>
#include <etoile/shrub/Shrub.hh>
#include <etoile/Exception.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/neutron/Index.hh>
#include <nucleus/neutron/Size.hh>
#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Entry.hh>

#include <elle/log.hh>

#include <Infinit.hh>
#include <Scheduler.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.Directory");

namespace etoile
{
  namespace wall
  {

//
// ---------- methods ---------------------------------------------------------
//

    gear::Identifier
    Directory::create()
    {
      ELLE_TRACE_FUNCTION("");

      gear::Scope* scope;
      gear::Directory* context;

      // acquire the scope.
      if (gear::Scope::Supply(scope) == elle::Status::Error)
        throw Exception("unable to supply the scope");

      gear::Guard guard(scope);
      gear::Identifier identifier;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor.
        guard.actor(new gear::Actor(scope));

        // return the identifier.
        identifier = guard.actor()->identifier;

        // apply the create automaton on the context.
        if (automaton::Directory::Create(*context) == elle::Status::Error)
          throw Exception("unable to create the directory");

        // set the actor's state.
        guard.actor()->state = gear::Actor::StateUpdated;

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        // waive the scope.
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");
      }

      return (identifier);
    }

    gear::Identifier
    Directory::load(path::Chemin const& chemin)
    {
      ELLE_TRACE_FUNCTION(chemin);

      gear::Scope* scope;
      gear::Directory* context;

      // acquire the scope.
      if (gear::Scope::Acquire(chemin, scope) == elle::Status::Error)
        throw Exception("unable to acquire the scope");

      gear::Guard guard(scope);
      gear::Identifier identifier;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor.
        guard.actor(new gear::Actor(scope));

        // return the identifier.
        identifier = guard.actor()->identifier;

        // locate the object based on the chemin.
        if (chemin.Locate(context->location) == elle::Status::Error)
          throw Exception("unable to locate the directory");

        ELLE_DEBUG("about to load the directory from the location '%s'",
                   context->location);

        try
          {
            // apply the load automaton on the context.
            if (automaton::Directory::Load(*context) == elle::Status::Error)
              throw Exception("unable to load the directory");
          }
        catch (std::exception const&)
          {
            Object::reload<gear::Directory>(*scope);
          }

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        // waive the scope.
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");
      }

      return (identifier);
    }

    void
    Directory::add(gear::Identifier const& parent,
                   path::Slab const& name,
                   gear::Identifier const& child)
    {
      ELLE_TRACE_FUNCTION(parent, name, child);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Directory* directory;
      gear::Object* object;
      nucleus::proton::Address address;

      // select the actor.
      if (gear::Actor::Select(child, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      // retrieve the scope.
      scope = actor->scope;

      // retrieve the context.
      if (scope->Use(object) == elle::Status::Error)
        throw Exception("unable to retrieve the context");

      // keep the address.
      address = object->location.address();

      // select the actor.
      if (gear::Actor::Select(parent, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      // retrieve the scope.
      scope = actor->scope;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(directory) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the add automaton on the context.
        if (automaton::Directory::Add(*directory,
                                      name,
                                      address) == elle::Status::Error)
          throw Exception("unable to add the directory entry");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }
    }

    ///
    /// this method returns the directory entry associated with the
    /// given name.
    ///
    /// note that this method should be used careful as a pointer to the
    /// target entry is returned. should this entry be destroyed by another
    /// actor's operation, accessing it could make the system crash.
    ///
    elle::Status        Directory::Lookup(
                          const gear::Identifier&               identifier,
                          const path::Slab&                     name,
                          nucleus::neutron::Entry const*& entry)
    {
      ELLE_TRACE_FUNCTION(identifier, name);

      gear::Actor*      actor;
      gear::Scope*      scope;
      gear::Directory*  context;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      // retrieve the scope.
      scope = actor->scope;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(), scope->mutex);

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the lookup automaton on the context.
        if (automaton::Directory::Lookup(*context,
                                         name,
                                         entry) == elle::Status::Error)
          throw Exception("unable to lookup the directory entry");
      }

      return elle::Status::Ok;
    }

    nucleus::neutron::Range<nucleus::neutron::Entry>
    Directory::consult(gear::Identifier const& identifier,
                       nucleus::neutron::Index const& index,
                       nucleus::neutron::Index const& size)
    {
      ELLE_TRACE_FUNCTION(identifier, index, size);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Directory* context;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      // retrieve the scope.
      scope = actor->scope;

      nucleus::neutron::Range<nucleus::neutron::Entry> range;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(), scope->mutex);

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the consult automaton on the context.
        if (automaton::Directory::Consult(*context,
                                          index,
                                          size,
                                          range) == elle::Status::Error)
          throw Exception("unable to consult the directory entries");
      }

      return (range);
    }

    ///
    /// this method renames a directory entry.
    ///
    elle::Status        Directory::Rename(
                          const gear::Identifier&               identifier,
                          const path::Slab&                     from,
                          const path::Slab&                     to)
    {
      ELLE_TRACE_FUNCTION(identifier, from, to);

      gear::Actor*      actor;
      gear::Scope*      scope;
      gear::Directory*  context;

      ELLE_TRACE("select the actor")
        if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
          throw Exception("unable to select the actor");

      ELLE_TRACE("lock the actor's scope for writing.");

      scope = actor->scope;
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        path::Venue venue(scope->chemin.venue);
        ELLE_TRACE("old venue: %s", venue);

        ELLE_TRACE("retrieve the context")
          if (scope->Use(context) == elle::Status::Error)
            throw Exception("unable to retrieve the context");

        ELLE_TRACE("apply the rename automaton on the context")
          if (automaton::Directory::Rename(*context,
                                           from,
                                           to) == elle::Status::Error)
            throw Exception("unable to rename the directory entry");

        actor->state = gear::Actor::StateUpdated;

        struct
        {
          path::Route           from;
          path::Route           to;
        }                       routes;

        // Create routes for both the _from_ and _to_ since these
        // routes are going to be used below several times.
        ELLE_TRACE("build the route for the previous revision of the entry")
          {
            if (routes.from.Create(scope->chemin.route, from) ==
                elle::Status::Error)
              throw Exception("unable to create the route");
            ELLE_TRACE("route: %s", routes.from);
          }

        ELLE_TRACE("build the route for the new revision of the entry")
          {
            if (routes.to.Create(scope->chemin.route, to) ==
                elle::Status::Error)
              throw Exception("unable to create the route");
            ELLE_TRACE("route: %s", routes.to);
          }

        // Update the scopes should some reference the renamed entry.
        //
        // Indeed, let us imagine the following scenario. a file
        // /tmp/F1 is created. this file is opened by two actors A and
        // B. then, actor A renames the file into /tmp/F2.
        //
        // Later one, a actor, say C, re-creates and releases /tmp/F1.
        // then C loads /tmp/F1. since the original scope for /tmp/F1
        // has not been updated and since actors remain, i.e A and B,
        // the original scope is retrieved instead of the new one.
        //
        // For this reason, the scopes must be updated.
        {
          struct
          {
            path::Chemin        from;
            path::Chemin        to;
          }                     chemins;

          if (chemins.from.Create(routes.from, venue) == elle::Status::Error)
            throw Exception("unable to create the chemin");

          // create the new chemin which includes the new route and
          // the venue, which has not changed since.
          if (chemins.to.Create(routes.to, venue) == elle::Status::Error)
            throw Exception("unable to create the chemin");

          // Update the scope so as to update all the scopes whose
          // chemins are now inconsistent---i.e referencing the old
          // chemin _chemin.from_.
          ELLE_TRACE("update the scope")
            if (gear::Scope::Update(chemins.from,
                                    chemins.to) == elle::Status::Error)
              throw Exception("unable to update the scopes");
        }

        //
        // invalidate the _from_ and _to_ routes from the shrub.
        //
        {
          // evict the route from the shrub.
          if (shrub::Shrub::Evict(routes.from) == elle::Status::Error)
            throw Exception("unable to evict the route from the shrub");

          // evict the route from the shrub.
          if (shrub::Shrub::Evict(routes.to) == elle::Status::Error)
            throw Exception("unable to evict the route from the shrub");
        }
      }

      return elle::Status::Ok;
    }

    ///
    /// this method removes a directory entry.
    ///
    elle::Status        Directory::Remove(
                          const gear::Identifier&               identifier,
                          const path::Slab&                     name)
    {
      ELLE_TRACE_FUNCTION(identifier, name);

      gear::Actor*      actor;
      gear::Scope*      scope;
      gear::Directory*  context;
      path::Route       route;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      // retrieve the scope.
      scope = actor->scope;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the remove automaton on the context.
        if (automaton::Directory::Remove(*context,
                                         name) == elle::Status::Error)
          throw Exception("unable to remove the directory entry");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;

        //
        // invalidate the route in the shrub.
        //
        {
          // build the route associated with the removed entry.
          if (route.Create(scope->chemin.route, name) == elle::Status::Error)
            throw Exception("unable to create the route");

          // evict the route from the shrub.
          if (shrub::Shrub::Evict(route) == elle::Status::Error)
            throw Exception("unable to evict the route from the shrub");
        }
      }

      return elle::Status::Ok;
    }

    void
    Directory::discard(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Directory* context;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      gear::Guard guard(actor);

      // retrieve the scope.
      scope = actor->scope;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // check the permissions before performing the operation in
        // order not to alter the scope should the operation not be
        // allowed.
        if (automaton::Rights::Operate(
              *context,
              gear::OperationDiscard) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                 "discarding this directory");

        // specify the closing operation performed by the actor.
        if (actor->Operate(gear::OperationDiscard) == elle::Status::Error)
          throw Exception("this operation cannot be performed by this actor");

        // delete the actor.
        guard.actor(nullptr);

        // specify the closing operation performed on the scope.
        if (scope->Operate(gear::OperationDiscard) == elle::Status::Error)
          throw Exception("unable to specify the operation being performed "
                 "on the scope");

        // trigger the shutdown.
        try
          {
            if (scope->Shutdown() == elle::Status::Error)
              throw Exception("unable to trigger the shutdown");
          }
        catch (elle::Exception const& e)
          {
            ELLE_ERR("unable to shutdown the scope: '%s'", e.what());
            return;
          }
      }

      // depending on the context's state.
      switch (context->state)
        {
        case gear::Context::StateDiscarded:
        case gear::Context::StateStored:
        case gear::Context::StateDestroyed:
          {
            //
            // if the directory has been sealed, i.e there is no more actor
            // operating on it, record it in the journal.
            //

            // relinquish the scope: at this point we know there is no
            // remaining actor.
            if (gear::Scope::Relinquish(scope) == elle::Status::Error)
              throw Exception("unable to relinquish the scope");

            // record the scope in the journal.
            if (journal::Journal::Record(scope) == elle::Status::Error)
              throw Exception("unable to record the scope in the journal");

            break;
          }
        default:
          {
            //
            // otherwise, some actors are probably still working on it.
            //

            break;
          }
        }
    }

    void
    Directory::store(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Directory* context;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      gear::Guard guard(actor);

      // retrieve the scope.
      scope = actor->scope;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // check the permissions before performing the operation in
        // order not to alter the scope should the operation not be
        // allowed.
        if (automaton::Rights::Operate(
              *context,
              gear::OperationStore) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                 "storing this directory");

        // specify the closing operation performed by the actor.
        if (actor->Operate(gear::OperationStore) == elle::Status::Error)
          throw Exception("this operation cannot be performed by this actor");

        // delete the actor.
        guard.actor(nullptr);

        // specify the closing operation performed on the scope.
        if (scope->Operate(gear::OperationStore) == elle::Status::Error)
          throw Exception("unable to specify the operation being performed "
                 "on the scope");

        // trigger the shutdown.
        try
          {
            if (scope->Shutdown() == elle::Status::Error)
              throw Exception("unable to trigger the shutdown");
          }
        catch (elle::Exception const& e)
          {
            ELLE_ERR("unable to shutdown the scope: '%s'", e.what());
            return;
          }
      }

      // depending on the context's state.
      switch (context->state)
        {
        case gear::Context::StateDiscarded:
        case gear::Context::StateStored:
        case gear::Context::StateDestroyed:
          {
            //
            // if the directory has been sealed, i.e there is no more actor
            // operating on it, record it in the journal.
            //

            // relinquish the scope: at this point we know there is no
            // remaining actor.
            if (gear::Scope::Relinquish(scope) == elle::Status::Error)
              throw Exception("unable to relinquish the scope");

            // record the scope in the journal.
            if (journal::Journal::Record(scope) == elle::Status::Error)
              throw Exception("unable to record the scope in the journal");

            break;
          }
        default:
          {
            //
            // otherwise, some actors are probably still working on it.
            //

            break;
          }
        }
    }

    ///
    /// this method destroys a directory.
    ///
    elle::Status        Directory::Destroy(
                          const gear::Identifier&               identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor*              actor;
      gear::Scope*              scope;
      gear::Directory*          context;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      gear::Guard               guard(actor);

      // retrieve the scope.
      scope = actor->scope;

      // Declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // check the permissions before performing the operation in
        // order not to alter the scope should the operation not be
        // allowed.
        if (automaton::Rights::Operate(
              *context,
              gear::OperationDestroy) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                 "destroying this directory");

        // specify the closing operation performed by the actor.
        if (actor->Operate(gear::OperationDestroy) == elle::Status::Error)
          throw Exception("this operation cannot be performed by this actor");

        // delete the actor.
        guard.actor(nullptr);

        // specify the closing operation performed on the scope.
        if (scope->Operate(gear::OperationDestroy) == elle::Status::Error)
          throw Exception("unable to specify the operation being performed "
                 "on the scope");

        // trigger the shutdown.
        try
          {
            if (scope->Shutdown() == elle::Status::Error)
              throw Exception("unable to trigger the shutdown");
          }
        catch (elle::Exception const& e)
          {
            ELLE_ERR("unable to shutdown the scope: '%s'", e.what());
            return elle::Status::Ok;
          }
      }

      // depending on the context's state.
      switch (context->state)
        {
        case gear::Context::StateDiscarded:
        case gear::Context::StateStored:
        case gear::Context::StateDestroyed:
          {
            //
            // if the directory has been sealed, i.e there is no more actor
            // operating on it, record it in the journal.
            //

            // relinquish the scope: at this point we know there is no
            // remaining actor.
            if (gear::Scope::Relinquish(scope) == elle::Status::Error)
              throw Exception("unable to relinquish the scope");

            // record the scope in the journal.
            if (journal::Journal::Record(scope) == elle::Status::Error)
              throw Exception("unable to record the scope in the journal");

            break;
          }
        default:
          {
            //
            // otherwise, some actors are probably still working on it.
            //

            break;
          }
        }

      return elle::Status::Ok;
    }
  }
}
