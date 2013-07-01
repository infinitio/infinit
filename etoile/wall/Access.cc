#include <elle/Exception.hh>
#include <elle/log.hh>

#include <reactor/scheduler.hh>

#include <etoile/Etoile.hh>
#include <etoile/wall/Access.hh>
#include <etoile/gear/Identifier.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/gear/Object.hh>
#include <etoile/automaton/Access.hh>
#include <etoile/Exception.hh>

#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/neutron/Index.hh>
#include <nucleus/neutron/Size.hh>
#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Permissions.hh>

#include <Infinit.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.Access");

namespace etoile
{
  namespace wall
  {
    nucleus::neutron::Record
    Access::lookup(etoile::Etoile& etoile,
                   const gear::Identifier& identifier,
                   const nucleus::neutron::Subject& subject)
    {
      ELLE_TRACE_FUNCTION(identifier, subject);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object* context;

      {
        reactor::Lock lock(scope->mutex);
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");
        nucleus::neutron::Record const* record(nullptr);
        if (automaton::Access::Lookup(*context, subject, record) ==
            elle::Status::Error)
          throw Exception("unable to lookup the record");
        if (record)
          return *record;
        else
          return nucleus::neutron::Record::null();
      }
    }

    nucleus::neutron::Range<nucleus::neutron::Record>
    Access::consult(etoile::Etoile& etoile,
                    gear::Identifier const& identifier,
                    nucleus::neutron::Index const& index,
                    nucleus::neutron::Size const& size)
    {
      ELLE_TRACE_FUNCTION(identifier, index, size);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object*     context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex);
        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the consult automaton on the context.
        nucleus::neutron::Range<nucleus::neutron::Record> records;
        if (automaton::Access::Consult(*context,
                                       index,
                                       size,
                                       records) == elle::Status::Error)
          throw Exception("unable to consult the access records");
        return records;
      }
    }

    ///
    /// this method grants the given access permissions to the subject.
    ///
    elle::Status        Access::Grant(
      etoile::Etoile& etoile,
      const gear::Identifier&               identifier,
      const nucleus::neutron::Subject& subject,
      const nucleus::neutron::Permissions& permissions)
    {
      ELLE_TRACE_FUNCTION(identifier, subject, permissions);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object*     context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());
        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the grant automaton on the context.
        if (automaton::Access::Grant(*context,
                                     subject,
                                     permissions) == elle::Status::Error)
          throw Exception("unable to grant access to the subject");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }

      return elle::Status::Ok;
    }

    ///
    /// this method removes the user's permissions from the access control
    /// list.
    ///
    elle::Status        Access::Revoke(
      etoile::Etoile& etoile,
      const gear::Identifier&               identifier,
      const nucleus::neutron::Subject& subject)
    {
      ELLE_TRACE_FUNCTION(identifier, subject);

      gear::Actor* actor = etoile.actor_get(identifier);
      std::shared_ptr<gear::Scope> scope = actor->scope;
      gear::Object*     context;

      // Declare a critical section.
      {
        reactor::Lock lock(scope->mutex.write());

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // apply the revoke automaton on the context.
        if (automaton::Access::Revoke(*context,
                                      subject) == elle::Status::Error)
          throw Exception("unable to revoke the subject's access permissions");

        // set the actor's state.
        actor->state = gear::Actor::StateUpdated;
      }
      return elle::Status::Ok;
    }

  }
}
