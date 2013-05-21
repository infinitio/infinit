#include <etoile/wall/Object.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/Guard.hh>
#include <etoile/gear/Object.hh>
#include <etoile/gear/File.hh>
#include <etoile/gear/Directory.hh>
#include <etoile/gear/Link.hh>
#include <etoile/automaton/Object.hh>
#include <etoile/automaton/Rights.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/journal/Journal.hh>
#include <etoile/abstract/Object.hh>
#include <etoile/shrub/Shrub.hh>
#include <etoile/path/Path.hh>
#include <etoile/Exception.hh>

#include <nucleus/proton/Location.hh>
#include <nucleus/neutron/Object.hh>
#include <nucleus/neutron/Genre.hh>

#include <elle/log.hh>
#include <elle/Exception.hh>

#include <Infinit.hh>
#include <Scheduler.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.Object");

namespace etoile
{
  namespace wall
  {

    gear::Identifier
    Object::load(const path::Chemin& chemin)
    {
      ELLE_TRACE_FUNCTION(chemin);

      gear::Scope* scope;
      gear::Object* context;

      // acquire the scope.
      if (gear::Scope::Acquire(chemin, scope) == elle::Status::Error)
        throw Exception("unable to acquire the scope");

      gear::Guard guard(scope);

      // XXX[tout ce bloc devrait probablement etre locke]

      // If the scope is new i.e there is no attached context, the system
      // needs to know what is the genre of the object, e.g directory, in
      // order allocate an appropriate context.
      if (scope->context == nullptr)
        {
          // In this case, the object is manually loaded in order to determine
          // the genre.
          nucleus::proton::Location location;
          std::unique_ptr<nucleus::neutron::Object> object;

          if (scope->chemin.Locate(location) == elle::Status::Error)
            throw Exception("unable to locate the object");

          try
            {
              object = depot::Depot::pull_object(location.address(),
                                                 location.revision());
            }
          catch (std::runtime_error& e)
            {
              assert(scope != nullptr);
              ELLE_TRACE("clearing the cache in order to evict %s",
                         scope->chemin.route)
                shrub::Shrub::clear();

              ELLE_TRACE("try to resolve the route now that the "
                         "cache was cleaned")
              {
                path::Venue venue;
                if (path::Path::Resolve(scope->chemin.route,
                                        venue) == elle::Status::Error)
                  throw Exception
                    (elle::sprintf("unable to resolve the route %s",
                                   scope->chemin.route));
                scope->chemin = path::Chemin(scope->chemin.route, venue);
                if (scope->chemin.Locate(location) == elle::Status::Error)
                  throw Exception("unable to locate the object");
              }

              ELLE_TRACE("trying to load the object again from %s", location)
                object = depot::Depot::pull_object(location.address(),
                                                   location.revision());
            }

          // Depending on the object's genre, a context is allocated
          // for the scope.
          switch (object->genre())
            {
            case nucleus::neutron::Genre::file:
              {
                gear::File* _context;

                if (scope->Use(_context) == elle::Status::Error)
                  throw Exception("unable to create the context");

                // In order to avoid loading the object twice, manually set it
                // in the context.
                //
                // !WARNING! this code is redundant with
                // automaton::Object::Load().
                _context->object.reset(object.release());
                _context->object->base(nucleus::proton::Base(*_context->object));
                _context->state = gear::Context::StateLoaded;

                break;
              }
            case nucleus::neutron::Genre::directory:
              {
                gear::Directory* _context;

                if (scope->Use(_context) == elle::Status::Error)
                  throw Exception("unable to create the context");

                // In order to avoid loading the object twice, manually set it
                // in the context.
                //
                // !WARNING! this code is redundant with
                // automaton::Object::Load().
                _context->object.reset(object.release());
                _context->object->base(nucleus::proton::Base(*_context->object));
                _context->state = gear::Context::StateLoaded;

                break;
              }
            case nucleus::neutron::Genre::link:
              {
                gear::Link* _context;

                if (scope->Use(_context) == elle::Status::Error)
                  throw Exception("unable to create the context");

                // In order to avoid loading the object twice, manually set it
                // in the context.
                //
                // !WARNING! this code is redundant with
                // automaton::Object::Load().
                _context->object.reset(object.release());
                _context->object->base(nucleus::proton::Base(*_context->object));
                _context->state = gear::Context::StateLoaded;

                break;
              }
            default:
              {
                // XXX[this whole code should probably be put within the
                //     critical section?]
                ELLE_STATEMENT(object.get()->Dump());
                throw Exception("unable to allocate the proper context");
              }
            }

          // At this point, the context represents the real object so
          // that, assuming it is a directory, both Object::* and
          // Directory::* methods could be used.
          ELLE_ASSERT(scope->context != nullptr);
        }

      // declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(),
                           scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor.
        guard.actor(new gear::Actor(scope));

        // return the identifier.
        gear::Identifier identifier = guard.actor()->identifier;

        // locate the object based on the chemin.
        if (scope->chemin.Locate(context->location) == elle::Status::Error)
          throw Exception("unable to locate the object");

        try
          {
            // apply the load automaton on the context.
            if (automaton::Object::Load(*context) == elle::Status::Error)
              throw Exception("unable to load the object");
          }
        catch (std::exception const& e)
          {
            ELLE_ASSERT(scope != nullptr);

            Object::reload<gear::Object>(*scope);
          }

        // waive the actor and the scope
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        return (identifier);
      }
    }

    abstract::Object
    Object::information(const gear::Identifier& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Object* context;

      // select the actor.
      if (gear::Actor::Select(identifier, actor) == elle::Status::Error)
        throw Exception("unable to select the actor");

      // retrieve the scope.
      scope = actor->scope;

      // declare a critical section.
      {
        reactor::Lock lock(infinit::scheduler(), scope->mutex);

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the information automaton on the context.
        abstract::Object abstract;
        if (automaton::Object::Information(*context,
                                           abstract) == elle::Status::Error)
          throw Exception("unable to retrieve general information "
                                "on the object");

        return abstract;
      }
    }

    void
    Object::discard(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Object* context;

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
          throw Exception("the user does not seem to have the necessary "
                                "permission for discarding this object");

        // specify the closing operation performed by the actor.
        if (actor->Operate(gear::OperationDiscard) == elle::Status::Error)
          throw Exception("this operation cannot be performed by "
                                "this actor");

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
            // if the object has been sealed, i.e there is no more actor
            // operating on it, record it in the journal.
            //

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

            // XXX[why isn't there such an assert in store and destroy also? there should be!]
            assert(!scope->actors.empty());
            break;
          }
        }
    }

    void
    Object::store(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Object* context;

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
                                   "storing this object");

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
            // if the object has been sealed, i.e there is no more actor
            // operating on it, record it in the journal.
            //

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
    Object::destroy(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor;
      gear::Scope* scope;
      gear::Object* context;

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
              gear::OperationDestroy) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                                   "destroying this object");

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
            // if the object has been sealed, i.e there is no more actor
            // operating on it, record it in the journal.
            //

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
  }
}
