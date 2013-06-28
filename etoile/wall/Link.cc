#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <etoile/Etoile.hh>
#include <etoile/wall/Link.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/Link.hh>
#include <etoile/gear/Operation.hh>
#include <etoile/gear/Guard.hh>
#include <etoile/automaton/Link.hh>
#include <etoile/automaton/Rights.hh>
#include <etoile/journal/Journal.hh>
#include <etoile/Exception.hh>

#include <Infinit.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.Link");

namespace etoile
{
  namespace wall
  {

//
// ---------- methods ---------------------------------------------------------
//

    gear::Identifier
    Link::create()
    {
      ELLE_TRACE_FUNCTION("");

      gear::Scope* scope;
      gear::Link* context;

      // acquire the scope.
      if (gear::Scope::Supply(scope) == elle::Status::Error)
        throw Exception("unable to supply the scope");

      gear::Guard guard(scope);
      gear::Identifier identifier;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor.
        guard.actor(new gear::Actor(scope));

        // return the identifier.
        identifier = guard.actor()->identifier;

        // apply the create automaton on the context.
        if (automaton::Link::Create(*context) == elle::Status::Error)
          throw Exception("unable to create the link");

        // set the actor's state.
        guard.actor()->state = gear::Actor::StateUpdated;

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        // waive the actor and the scope.
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");
      }

      ELLE_DEBUG("returning identifier %s", identifier);

      return (identifier);
    }

    ///
    /// this method loads a link object given its chemin and initializes
    /// an associated context.
    ///
    elle::Status        Link::Load(
                          const path::Chemin&                   chemin,
                          gear::Identifier&                     identifier)
    {
      ELLE_TRACE_FUNCTION(chemin);

      gear::Scope*      scope;
      gear::Link*       context;

      // acquire the scope.
      if (gear::Scope::Acquire(chemin, scope) == elle::Status::Error)
        throw Exception("unable to acquire the scope");

      gear::Guard               guard(scope);

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor.
        guard.actor(new gear::Actor(scope));

        // return the identifier.
        identifier = guard.actor()->identifier;

        // locate the object based on the chemin.
        context->location = chemin.locate();

        ELLE_DEBUG("about to load the directory from the location '%s'",
                   context->location);

        try
          {
            // apply the load automaton on the context.
            if (automaton::Link::Load(*context) == elle::Status::Error)
              throw Exception("unable to load the link");
          }
        catch (std::exception const&)
          {
            assert(scope != nullptr);
            Object::reload<gear::Link>(*scope);
          }

        ELLE_DEBUG("returning identifier %s on %s", identifier, *scope);

        // waive the actor and the scope
        if (guard.Release() == elle::Status::Error)
          throw Exception("unable to release the guard");
      }

      return elle::Status::Ok;
    }

    void
    Link::bind(gear::Identifier const& identifier,
               std::string const& target)
    {
      ELLE_TRACE_FUNCTION(identifier, target);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      gear::Scope* scope = actor->scope;
      gear::Link* context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the bind automaton on the context.
        if (automaton::Link::Bind(*context, target) == elle::Status::Error)
          throw Exception("unable to bind the link");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }
    }

    std::string
    Link::resolve(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      gear::Scope* scope = actor->scope;
      gear::Link* context;

      std::string target;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex);

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the resolve automaton on the context.
        if (automaton::Link::Resolve(*context,
                                     target) == elle::Status::Error)
          throw Exception("unable to resolve the link");
      }

      return (target);
    }

    ///
    /// this method discards the scope, potentially ignoring the
    /// performed modifications.
    ///
    elle::Status        Link::Discard(
                          const gear::Identifier&               identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      gear::Scope* scope = actor->scope;
      gear::Link*       context;

      gear::Guard               guard(actor);

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
          throw Exception("the user does not seem to have the necessary permission for "
                 "discarding this link");

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
            // if the link has been sealed, i.e there is no more actor
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

      return elle::Status::Ok;
    }

    void
    Link::store(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      gear::Scope* scope = actor->scope;
      gear::Link* context;

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
                 "storing this link");

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
            // if the link has been sealed, i.e there is no more actor
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

    ///
    /// this method destroys a link.
    ///
    elle::Status        Link::Destroy(
                          const gear::Identifier&               identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      gear::Scope* scope = actor->scope;
      gear::Link*       context;

      gear::Guard               guard(actor);

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
                 "destroying this link");

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
            // if the link has been sealed, i.e there is no more actor
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

      return elle::Status::Ok;
    }
  }
}
