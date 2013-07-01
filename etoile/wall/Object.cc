#include <elle/log.hh>
#include <elle/Exception.hh>

#include <reactor/scheduler.hh>

#include <etoile/Etoile.hh>
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

ELLE_LOG_COMPONENT("infinit.etoile.wall.Object");

namespace etoile
{
  namespace wall
  {

    gear::Identifier
    Object::load(etoile::Etoile& etoile,
                 const path::Chemin& chemin)
    {
      ELLE_TRACE_FUNCTION(chemin);

      std::shared_ptr<gear::Scope> scope =
        etoile.scope_acquire(chemin);
      gear::Guard guard(scope);
      gear::Object* context;

      // XXX[tout ce bloc devrait probablement etre locke]

      // If the scope is new i.e there is no attached context, the system
      // needs to know what is the genre of the object, e.g directory, in
      // order allocate an appropriate context.
      if (scope->context == nullptr)
        {
          // In this case, the object is manually loaded in order to determine
          // the genre.
          nucleus::proton::Location location = scope->chemin.locate();
          std::unique_ptr<nucleus::neutron::Object> object;

          try
            {
              object = etoile.depot().pull_object(
                location.address(), location.revision());
            }
          catch (std::runtime_error& e)
            {
              assert(scope != nullptr);
              ELLE_TRACE("clearing the cache in order to evict %s",
                         scope->chemin.route())
                etoile.shrub().clear();

              ELLE_TRACE("try to resolve the route now that the "
                         "cache was cleaned")
              {
                path::Venue venue =
                  path::Path::Resolve(*Etoile::instance(),
                                      scope->chemin.route());
                scope->chemin = path::Chemin(scope->chemin.route(), venue);
                location = scope->chemin.locate();
              }

              ELLE_TRACE("trying to load the object again from %s", location)
                object = etoile.depot().pull_object(
                  location.address(), location.revision());
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
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor.
        guard.actor(new gear::Actor(scope));

        // return the identifier.
        gear::Identifier identifier = guard.actor()->identifier;

        // locate the object based on the chemin.
        context->location = scope->chemin.locate();

        try
          {
            // apply the load automaton on the context.
            if (automaton::Object::Load(*context) == elle::Status::Error)
              throw Exception("unable to load the object");
          }
        catch (std::exception const& e)
          {
            ELLE_ASSERT(scope != nullptr);

            Object::reload<gear::Object>(etoile, *scope);
          }

        // waive the actor and the scope
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        return (identifier);
      }
    }

    abstract::Object
    Object::information(etoile::Etoile& etoile,
                        const gear::Identifier& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;

      {
        reactor::Lock lock(scope->mutex);

        // retrieve the context.
        gear::Object* context;
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
    Object::discard(etoile::Etoile& etoile,
                    gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      gear::Guard guard(actor);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

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
            if (journal::Journal::Record(std::move(scope)) == elle::Status::Error)
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
    Object::store(etoile::Etoile& etoile,
                  gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      gear::Guard guard(actor);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

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
            if (journal::Journal::Record(std::move(scope)) == elle::Status::Error)
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
    Object::destroy(etoile::Etoile& etoile,
                    gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      gear::Guard guard(actor);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

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
            if (journal::Journal::Record(std::move(scope)) == elle::Status::Error)
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
