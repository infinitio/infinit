#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <etoile/Etoile.hh>
#include <etoile/wall/Attributes.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/Object.hh>
#include <etoile/automaton/Attributes.hh>
#include <etoile/Exception.hh>

#include <nucleus/neutron/Trait.hh>
#include <nucleus/neutron/Range.hh>

#include <Infinit.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.Attributes");

namespace etoile
{
  namespace wall
  {

//
// ---------- static methods --------------------------------------------------
//

    void
    Attributes::set(gear::Identifier const& identifier,
                    elle::String const& name,
                    elle::String const& value)
    {
      ELLE_TRACE_FUNCTION(identifier, name, value);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the set automaton on the context.
        if (automaton::Attributes::Set(*context,
                                       name,
                                       value) == elle::Status::Error)
          throw Exception("unable to set the attribute");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }
    }

    nucleus::neutron::Trait
    Attributes::get(gear::Identifier const& identifier,
                    elle::String const& name)
    {
      ELLE_TRACE_FUNCTION(identifier, name);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      nucleus::neutron::Trait const* trait = nullptr;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex);

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the get automaton on the context.
        if (automaton::Attributes::Get(*context,
                                       name,
                                       trait) == elle::Status::Error)
          throw Exception("unable to get the attribute");
      }

      // Return the trait according to the automaton's result.
      if (trait != nullptr)
        return (*trait);
      else
        return (nucleus::neutron::Trait::null());
    }

    nucleus::neutron::Range<nucleus::neutron::Trait>
    Attributes::fetch(gear::Identifier const& identifier)
    {
      ELLE_TRACE_FUNCTION(identifier);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      nucleus::neutron::Range<nucleus::neutron::Trait> range;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex);

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the fetch automaton on the context.
        if (automaton::Attributes::Fetch(*context,
                                         range) == elle::Status::Error)
          throw Exception("unable to fetch the attribute");
      }

      return (range);
    }

    ///
    /// this method removes the given attribute from the list.
    ///
    elle::Status        Attributes::Omit(
                          const gear::Identifier&               identifier,
                          const elle::String&                   name)
    {
      ELLE_TRACE_FUNCTION(identifier, name);

      gear::Actor* actor = Etoile::instance()->actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object*     context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the omit automaton on the context.
        if (automaton::Attributes::Omit(*context,
                                        name) == elle::Status::Error)
          throw Exception("unable to omit the attribute");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }

      return elle::Status::Ok;
    }

  }
}
