#include <etoile/automaton/Link.hh>
#include <etoile/automaton/Object.hh>
#include <etoile/automaton/Contents.hh>
#include <etoile/automaton/Rights.hh>
#include <etoile/gear/Link.hh>
#include <etoile/path/Way.hh>
#include <etoile/Exception.hh>

#include <agent/Agent.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.etoile.automaton.Link");

namespace etoile
{
  namespace automaton
  {

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method creates a link object within the given context.
    ///
    elle::Status        Link::Create(
                          gear::Link&                           context)
    {
      ELLE_TRACE_FUNCTION(context);

      ELLE_ASSERT(context.object == nullptr);

      context.object.reset(
        new nucleus::neutron::Object(nucleus::proton::Network(Infinit::Network),
                                     agent::Agent::keypair().K(),
                                     nucleus::neutron::Genre::link));

      nucleus::proton::Address address(context.object->bind());

      // create the context's location with an initial revision number.
      context.location =
        nucleus::proton::Location(address, context.object->revision());

      // set the context's state.
      context.state = gear::Context::StateCreated;

      return elle::Status::Ok;
    }

    ///
    /// this method loads an existing link object identified by the
    /// given location.
    ///
    elle::Status        Link::Load(
                          gear::Link&                           context)
    {
      ELLE_TRACE_FUNCTION(context);

      // return if the context has already been loaded.
      if (context.state != gear::Context::StateUnknown)
        return elle::Status::Ok;

      // load the object.
      if (Object::Load(context) == elle::Status::Error)
        throw Exception("unable to fetch the object");

      // check that the object is a link.
      if (context.object->genre() != nucleus::neutron::Genre::link)
        throw Exception("this object does not seem to be a link");

      // set the context's state.
      context.state = gear::Context::StateLoaded;

      return elle::Status::Ok;
    }

    ///
    /// this method binds a new target to the link.
    ///
    elle::Status        Link::Bind(
                          gear::Link&                           context,
                          const path::Way&                      way)
    {
      ELLE_TRACE_FUNCTION(context, way);

      // determine the rights.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // check if the current user has the right the bind the link.
      if ((context.rights.permissions & nucleus::neutron::permissions::write) !=
          nucleus::neutron::permissions::write)
        throw Exception("the user does not seem to have the permission to bind "
               "this link");

      // open the contents.
      if (Contents::Open(context) == elle::Status::Error)
        throw Exception("unable to open the contents");

      ELLE_ASSERT(false);
      /* XXX[porcupine]
      // check that the content exists: the subject may have lost the
      // read permission between the previous check and the Contents::Open().
      if (context.contents->content == nullptr)
        throw Exception("the user does not seem to be able to operate on this "
               "link");

      // bind the link.
      if (context.contents->content->Bind(way.path) == elle::Status::Error)
        throw Exception("unable to bind the link");

      // retrieve the new contents's size.
      if (context.contents->content->Capacity(size) == elle::Status::Error)
        throw Exception("unable to retrieve the contents's size");

      // update the object.
      if (context.object->Update(
            context.object->author(),
            context.object->contents(),
            size,
            context.object->access(),
            context.object->owner_token()) == elle::Status::Error)
        throw Exception("unable to update the object");
      */
      // set the context's state.
      context.state = gear::Context::StateModified;

      return elle::Status::Ok;
    }

    ///
    /// this method returns the way associated with this link.
    ///
    elle::Status        Link::Resolve(
                          gear::Link&                           context,
                          path::Way&                            way)
    {
      ELLE_TRACE_FUNCTION(context);

      // determine the rights.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // check if the current user has the right the resolve the link..
      if ((context.rights.permissions & nucleus::neutron::permissions::read) !=
          nucleus::neutron::permissions::read)
        throw Exception("the user does not seem to have the permission to resolve "
               "this link");

      // open the contents.
      if (Contents::Open(context) == elle::Status::Error)
        throw Exception("unable to open the contents");

      ELLE_ASSERT(false);
      /* XXX[porcupine]
      // check that the content exists: the subject may have lost the
      // read permission between the previous check and the Contents::Open().
      if (context.contents->content == nullptr)
        throw Exception("the user does not seem to be able to operate on this "
               "link");

      // resolve the link.
      if (context.contents->content->Resolve(way.path) == elle::Status::Error)
        throw Exception("unable to resolve the link");
      */
      return elle::Status::Ok;
    }

    ///
    /// this method is called whenever the context is being closed without
    /// any modification having been performed.
    ///
    elle::Status        Link::Discard(
                          gear::Link&                           context)
    {
      ELLE_TRACE_FUNCTION(context);

      // discard the object-related information.
      if (Object::Discard(context) == elle::Status::Error)
        throw Exception("unable to discard the object");

      // set the context's state.
      context.state = gear::Context::StateDiscarded;

      return elle::Status::Ok;
    }

    ///
    /// this method destroys the link by marking all the blocks
    /// as dying for future removal.
    ///
    elle::Status        Link::Destroy(
                          gear::Link&                           context)
    {
      ELLE_TRACE_FUNCTION(context);

      // determine the rights.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // check if the current user is the object owner.
      if (context.rights.role != nucleus::neutron::Object::RoleOwner)
        throw Exception("the user does not seem to have the permission "
                              "to destroy this link");

      // destroy the contents.
      if (Contents::Destroy(context) == elle::Status::Error)
        throw Exception("unable to destroy the contents");

      // destroy the object-related information.
      if (Object::Destroy(context) == elle::Status::Error)
        throw Exception("unable to destroy the object");

      // set the context's state.
      context.state = gear::Context::StateDestroyed;

      return elle::Status::Ok;
    }

    ///
    /// this method terminates the automaton by making the whole link
    /// consistent according to the set of modifications having been performed.
    ///
    elle::Status        Link::Store(
                          gear::Link&                           context)
    {
      ELLE_TRACE_FUNCTION(context);

      // close the contents.
      if (Contents::Close(context) == elle::Status::Error)
        throw Exception("unable to close the contents");

      // store the object-related information.
      if (Object::Store(context) == elle::Status::Error)
        throw Exception("unable to store the object");

      // set the context's state.
      context.state = gear::Context::StateStored;

      return elle::Status::Ok;
    }

  }
}
