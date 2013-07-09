#include <elle/log.hh>
#include <elle/Buffer.hh>

#include <reactor/scheduler.hh>

#include <etoile/Etoile.hh>
#include <etoile/wall/File.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/File.hh>
#include <etoile/gear/Operation.hh>
#include <etoile/gear/Guard.hh>
#include <etoile/automaton/File.hh>
#include <etoile/automaton/Rights.hh>
#include <etoile/journal/Journal.hh>
#include <etoile/abstract/Object.hh>
#include <etoile/Exception.hh>

#include <nucleus/neutron/Offset.hh>
#include <nucleus/neutron/Size.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.File");

namespace etoile
{
  namespace wall
  {
    gear::Identifier
    File::create(etoile::Etoile& etoile)
    {
      ELLE_TRACE_FUNCTION("");

      std::shared_ptr<gear::Scope> scope = etoile.scope_supply();
      gear::Guard guard(scope);
      gear::File* context;
      gear::Identifier identifier;

      // retrieve the context.
      if (scope->Use(etoile, context) == elle::Status::Error)
        throw Exception("unable to retrieve the context");

      guard.actor(new gear::Actor(scope));

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // return the identifier.
        identifier = guard.actor()->identifier;

        // apply the create automaton on the context.
        if (automaton::File::Create(*context) == elle::Status::Error)
          throw Exception("unable to create the file");

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
    File::load(etoile::Etoile& etoile,
               path::Chemin const& chemin)
    {
      ELLE_TRACE_FUNCTION(chemin);

      std::shared_ptr<gear::Scope> scope =
        etoile.scope_acquire(chemin);
      gear::Guard guard(scope);
      gear::File* context;
      gear::Identifier identifier;

      // retrieve the context.
      if (scope->Use(etoile, context) == elle::Status::Error)
        throw Exception("unable to retrieve the context");

      // allocate an actor.
      guard.actor(new gear::Actor(scope));

      // declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // return the identifier.
        identifier = guard.actor()->identifier;

        // locate the object based on the chemin.
        context->location = chemin.locate();

        ELLE_DEBUG("about to load the directory from the location '%s'",
                   context->location);

        try
        {
          if (automaton::File::Load(*context) == elle::Status::Error)
            throw Exception("unable to load the file");
        }
        catch (elle::Exception const&)
        {
          assert(scope != nullptr);
          Object::reload<gear::File>(etoile, *scope);
        }

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        // waive the actor and the scope.
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");
      }

      return (identifier);
    }

    void
    File::write(etoile::Etoile& etoile,
                gear::Identifier const& identifier,
                nucleus::neutron::Offset const& offset,
                elle::ConstWeakBuffer data)
    {
      ELLE_TRACE_FUNCTION(identifier, offset, data);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::File* context;

      // declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(etoile, context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the write automaton on the context.
        if (automaton::File::Write(*context,
                                   offset,
                                   data) == elle::Status::Error)
          throw Exception("unable to write the file");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }
    }

    elle::Buffer
    File::read(etoile::Etoile& etoile,
               gear::Identifier const& identifier,
               nucleus::neutron::Offset const& offset,
               nucleus::neutron::Size const& size)
    {
      ELLE_TRACE_FUNCTION(identifier, offset, size);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::File* context;

      elle::Buffer* buffer = nullptr;

      ELLE_FINALLY_ACTION_DELETE(buffer);

      // declare a critical section.
      {
        reactor::Lock lock(scope->mutex);

        // retrieve the context.
        if (scope->Use(etoile, context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the read automaton on the context.
        buffer = new elle::Buffer{std::move(automaton::File::read(*context,
                                                                  offset,
                                                                  size))};
      }

      // Thanks to the finally, the buffer will be deleted after being returned.
      //
      // Besides, the buffer is moved so as to make sure no copy is performed.
      // We are forced to do so because Buffers are no-copyable by default.
      return (std::move(*buffer));
    }

    /// This method adjusts the size of a file.
    void
    File::adjust(etoile::Etoile& etoile,
                 const gear::Identifier& identifier,
                 const nucleus::neutron::Size& size)
    {
      ELLE_TRACE_FUNCTION(identifier, size);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::File* context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(etoile, context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the adjust automaton on the context.
        if (automaton::File::Adjust(*context,
                                    size) == elle::Status::Error)
          throw Exception("unable to adjust the file's size");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }
    }

    void
    File::discard(etoile::Etoile& etoile,
                  gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::File* context;

      gear::Guard guard(actor);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(etoile, context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // check the permissions before performing the operation in
        // order not to alter the scope should the operation not be
        // allowed.
        if (automaton::Rights::Operate(
              *context,
              gear::OperationDiscard) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                 "discarding this file");

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
          // If the file has been sealed, i.e there is no more actor
          // operating on it, record it in the journal.
          if (journal::Journal::Record(std::move(scope)) == elle::Status::Error)
            throw Exception("unable to record the scope in the journal");

          break;
        }
        default:
        {
          // Otherwise, some actors are probably still working on it.
          break;
        }
      }
    }

    void
    File::store(etoile::Etoile& etoile,
                gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::File* context;

      gear::Guard guard(actor);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(etoile, context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // check the permissions before performing the operation in
        // order not to alter the scope should the operation not be
        // allowed.
        if (automaton::Rights::Operate(
              *context,
              gear::OperationStore) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                 "storing this file");

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
          // If the file has been sealed, i.e there is no more actor
          // operating on it, record it in the journal.
          if (journal::Journal::Record(std::move(scope)) == elle::Status::Error)
            throw Exception("unable to record the scope in the journal");

          break;
        }
        default:
        {
          // Otherwise, some actors are probably still working on it.
            break;
        }
      }
    }

    ///
    /// this method destroys a file.
    ///
    void
    File::destroy(etoile::Etoile& etoile,
                  const gear::Identifier& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::File* context;

      gear::Guard guard(actor);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(etoile, context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // check the permissions before performing the operation in
        // order not to alter the scope should the operation not be
        // allowed.
        if (automaton::Rights::Operate(
              *context,
              gear::OperationDestroy) == elle::Status::Error)
          throw Exception("the user does not seem to have the necessary permission for "
                 "destroying this file");

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
          // If the file has been sealed, i.e there is no more actor
          // operating on it, record it in the journal.
          if (journal::Journal::Record(std::move(scope)) == elle::Status::Error)
            throw Exception("unable to record the scope in the journal");

          break;
        }
        default:
        {
          // Otherwise, some actors are still working on it.
          break;
        }
      }
    }
  }
}
