#include <elle/log.hh>

#include <etoile/automaton/Access.hh>
#include <etoile/automaton/Rights.hh>
#include <etoile/depot/Depot.hh>
#include <etoile/gear/Object.hh>
#include <etoile/gear/Action.hh>
#include <etoile/nest/Nest.hh>
#include <etoile/Exception.hh>

#include <nucleus/proton/Address.hh>
#include <nucleus/proton/Revision.hh>
#include <nucleus/proton/State.hh>
#include <nucleus/proton/Porcupine.hh>
#include <nucleus/neutron/Access.hh>
#include <nucleus/neutron/Permissions.hh>
#include <nucleus/neutron/Subject.hh>
#include <nucleus/neutron/Record.hh>
#include <nucleus/neutron/Index.hh>
#include <nucleus/neutron/Size.hh>
#include <nucleus/neutron/Range.hh>
#include <nucleus/neutron/Token.hh>
#include <nucleus/neutron/Object.hh>

#include <hole/Hole.hh>

ELLE_LOG_COMPONENT("infinit.etoile.automaton.Access");

namespace etoile
{
  namespace automaton
  {

    ///
    /// this method opens the access block by loading it if necessary i.e
    /// if the object references such a block.
    ///
    /// if not such block is referenced, an empty access is author .
    ///
    elle::Status        Access::Open(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      // XXX
      static cryptography::SecretKey secret_key(cryptography::cipher::Algorithm::aes256,
                                                ACCESS_SECRET_KEY);

      // check if the access has already been opened.
      if (context.access_porcupine != nullptr)
        return elle::Status::Ok;

      // Check if there exists access. if so, instanciate a porcupine.
      if (context.object->access().empty() == false)
        {
          // Instanciate a nest.
          context.access_nest =
            new etoile::nest::Nest(context.etoile(),
                                   ACCESS_SECRET_KEY_LENGTH,
                                   context.access_limits,
                                   context.etoile().network(),
                                   context.etoile().user_subject().user(),
                                   context.access_threshold);

          // Instanciate a porcupine.
          context.access_porcupine =
            new nucleus::proton::Porcupine<nucleus::neutron::Access>{
              context.object->access(),
              secret_key,
              *context.access_nest};
        }
      else
        {
          // Instanciate a nest.
          context.access_nest =
            new etoile::nest::Nest(context.etoile(),
                                   ACCESS_SECRET_KEY_LENGTH,
                                   context.access_limits,
                                   context.etoile().network(),
                                   context.etoile().user_subject().user(),
                                   context.access_threshold);

          // otherwise create a new empty porcupine.
          context.access_porcupine =
            new nucleus::proton::Porcupine<nucleus::neutron::Access>{
              *context.access_nest};
        }

      ELLE_ASSERT(context.access_porcupine != nullptr);

      return elle::Status::Ok;
    }

    ///
    /// this method adds a record to the access block, granting access
    /// to the given subject.
    ///
    elle::Status        Access::Grant(
                          gear::Object&                         context,
                          const nucleus::neutron::Subject& subject,
                          const nucleus::neutron::Permissions& permissions)
    {
      ELLE_TRACE_FUNCTION(context, subject, permissions);

      // determine the rights over the object.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // verify that the user can modify the accesses.
      if (context.rights.role != nucleus::neutron::Object::RoleOwner)
        throw Exception("the user does not seem to have the permission to modify "
               "the access permissions");

      // update the accesses depending on the subject.
      if (subject == context.object->owner_subject())
        {
          //
          // in this case, the subject represents the object's owner.
          //

          ELLE_TRACE("the target subject is the Object owner");

          // update the permissions.
          if (context.object->Administrate(
                context.object->attributes(),
                permissions) == elle::Status::Error)
            throw Exception("unable to administrate the object");
        }
      else
        {
          //
          // otherwise, the subject is a lord being a user or a group.
          //

          ELLE_TRACE("the target subject is _not_ the Object owner");

          // open the access block.
          if (Access::Open(context) == elle::Status::Error)
            throw Exception("unable to open the access block");

          // Retrieve a door on the access.
          nucleus::proton::Door<nucleus::neutron::Access> door{
            context.access_porcupine->lookup(subject)};

          door.open();

          if (door().exist(subject) == true)
            {
              ELLE_TRACE("the target subject exists in the Access block");

              // update the access block according to the given permissions.
              if (permissions == nucleus::neutron::permissions::none)
                {
                  //
                  // in this case, the subject is being removed all his
                  // permissions.
                  //
                  // therefore, rather than keeping an access record with
                  // no permission, the record is removed.
                  //
                  // this way, the access control list is simplified,
                  // potentially reduced to an empty list in which case
                  // the Access block would be removed.
                  //

                  ELLE_TRACE("the target subject has no permission");

                  // remove the record associated with the given subject.
                  door().erase(subject);

                  // the object must be marked as administered i.e dirty so
                  // that the meta signature gets re-computed i.e the access
                  // fingerprint has probably changed.
                  //
                  // for more information, please refer to the Object class.
                  if (context.object->Administrate(
                        context.object->attributes(),
                        context.object->owner_permissions()) == elle::Status::Error)
                    throw Exception("unable to administrate the object");
                }
              else
                {
                  //
                  // in this case, the access record can be updated.
                  //

                  ELLE_TRACE("the target subject has some permissions over "
                             "the Object");

                  switch (subject.type())
                    {
                    case nucleus::neutron::Subject::TypeUser:
                      {
                        ELLE_TRACE("update the Access user record");

                        // Create a token based on the fact that
                        // the object does or does not have content.
                        //
                        // If there is no data, set the token as null,
                        // otherwise, compute it base on the key which
                        // has been extracted from the owner's token
                        // i.e rights.key.
                        nucleus::neutron::Token token =
                          context.object->contents().empty() == true ?
                          nucleus::neutron::Token::null() :
                          nucleus::neutron::Token(*context.rights.key,
                                                  subject.user());

                        door().update(subject, permissions);
                        door().update(subject, token);

                        break;
                      }
                    case nucleus::neutron::Subject::TypeGroup:
                      {
                        std::unique_ptr<nucleus::neutron::Group> group;

                        ELLE_TRACE("fetch the Group block from the storage");

                        // XXX[the context should make use of unique_ptr instead
                        //     of releasing here.]
                        group =
                          context.etoile().depot().pull_group(
                            subject.group(),
                            nucleus::proton::Revision::Last);

                        ELLE_TRACE("update the Access group record");

                        nucleus::neutron::Token token =
                          context.object->contents().empty() == true ?
                          nucleus::neutron::Token::null() :
                          nucleus::neutron::Token(*context.rights.key,
                                                  group->pass_K());

                        door().update(subject, permissions);
                        door().update(subject, token);

                        break;
                      }
                    default:
                      throw Exception
                        (elle::sprintf("invalid subject type '%u'",
                                       subject.type()));
                    }
                }
            }
          else
            {
              nucleus::neutron::Record* record = nullptr;

              // XXX: It not safe.
              elle::SafeFinally delete_record{ [&] { delete record; } };

              ELLE_TRACE("the target subject is _not_ present in the "
                         "Access block");

              switch (subject.type())
                {
                case nucleus::neutron::Subject::TypeUser:
                  {
                    ELLE_TRACE("generate a new user record");

                    nucleus::neutron::Token token =
                      context.object->contents().empty() == true ?
                      nucleus::neutron::Token::null() :
                      nucleus::neutron::Token(*context.rights.key,
                                              subject.user());

                    // allocate a new record.
                    record = new nucleus::neutron::Record{subject,
                                                          permissions,
                                                          token};

                    break;
                  }
                case nucleus::neutron::Subject::TypeGroup:
                  {
                    std::unique_ptr<nucleus::neutron::Group> group;

                    ELLE_TRACE("fetch the Group block");

                    // XXX[the context should make use of unique_ptr instead
                    //     of releasing here.]
                    group =
                      context.etoile().depot().pull_group(
                        subject.group(),
                        nucleus::proton::Revision::Last);

                    ELLE_TRACE("generate a new group record");

                    nucleus::neutron::Token token =
                      context.object->contents().empty() == true ?
                      nucleus::neutron::Token::null() :
                      nucleus::neutron::Token(*context.rights.key,
                                              group->pass_K());

                    // allocate a new record.
                    record = new nucleus::neutron::Record{subject,
                                                          permissions,
                                                          token};

                    break;
                  }
                default:
                  throw Exception
                    (elle::sprintf("invalid subject type '%u'",
                                   subject.type()));
                }

              ELLE_ASSERT(record != nullptr);

              ELLE_TRACE("add the record to the Access block");
              door().insert(record);

              delete_record.abort();
            }

          door.close();

          // Update the porcupine.
          context.access_porcupine->update(subject);

          ELLE_TRACE("administrate the Object so as to mark it as dirty");

          // in any case, the object must be marked as administered i.e dirty
          // so that the meta signature gets re-computed i.e the access
          // fingerprint has probably changed.
          //
          // for more information, please refer to the Object class.
          if (context.object->Administrate(
                context.object->attributes(),
                context.object->owner_permissions()) == elle::Status::Error)
            throw Exception("unable to administrate the object");
        }

      ELLE_TRACE("audit the Object");

      // try to audit the object because the current author may have
      // lost its write permission in the process.
      if (Access::Audit(context, subject) == elle::Status::Error)
        throw Exception("unable to audit the object");

      // is the target subject the user i.e the object owner in this case.
      if (context.etoile().user_subject() == subject)
        {
          // update the context rights.
          if (Rights::Update(context, permissions) == elle::Status::Error)
            throw Exception("unable to update the rigths");
        }

      // set the context's state.
      context.state = gear::Context::StateModified;

      return elle::Status::Ok;
    }

    ///
    /// this method looks for the given subject in the accesses and
    /// return the associated permissions.
    ///
    elle::Status        Access::Lookup(
                          gear::Object&                         context,
                          const nucleus::neutron::Subject& subject,
                          nucleus::neutron::Record const*& record)
    {
      ELLE_TRACE_FUNCTION(context, subject);

      // try to make the best of this call.
      if (context.etoile().user_subject() == subject)
        {
          //
          // indeed, if the target subject is the current user, determine
          // the user's rights so that this is not to be done later.
          //

          ELLE_TRACE("the target subject is the current user");

          // determine the user's rights on the object.
          if (Rights::Determine(context) == elle::Status::Error)
            throw Exception("unable to determine the user's rights");

          // return the record, if present.
          if (context.rights.role != nucleus::neutron::Object::RoleNone)
            record = context.rights.record;
          else
            record = nullptr;
        }
      else
        {
          //
          // otherwise, proceed normally.
          //

          ELLE_TRACE("the target subject is _not_ the current user");

          // perform the lookup according to the subject.
          if (subject == context.object->owner_subject())
            {
              //
              // if the target subject is the object owner, retrieve the
              // access record from the owner's meta section. note that
              // this record is not part of the object but has been generated
              // automatically when the object was extracted.
              //

              ELLE_TRACE("the target subject is the object owner");

              // return the record.
              record = &context.object->owner_record();

              ELLE_TRACE("Access lookup found record %p from "
                             "context.object.meta.owner", record);
            }
          else
            {
              //
              // if we are dealing with a lord, open the access block
              // in look in it.
              //

              ELLE_TRACE("the target subject is _not_ the object owner: "
                             "look in the Access block");

              // open the access.
              if (Access::Open(context) == elle::Status::Error)
                throw Exception("unable to open the access block");

              // Retrieve a door on the access.
              nucleus::proton::Door<nucleus::neutron::Access> door{
                context.access_porcupine->lookup(subject)};

              door.open();

              // XXX[does the record exist: if not, return a null pointer]
              if (door().exist(subject) == false)
                {
                  record = nullptr;
                }
              else
                {
                  // Look up the record.
                  // XXX[we take the address of the reference: wrong]
                  record = &(door().locate(subject));
                }

              door.close();
            }
        }

      return elle::Status::Ok;
    }

    ///
    /// this method returns a subset---i.e a range---of the access block.
    ///
    elle::Status        Access::Consult(
                          gear::Object& context,
                          const nucleus::neutron::Index& index,
                          const nucleus::neutron::Size& size,
                          nucleus::neutron::Range<
                            nucleus::neutron::Record>& range)
    {
      ELLE_TRACE_FUNCTION(context, index, size);

      nucleus::neutron::Index _index = index;

      if (Access::Open(context) == elle::Status::Error)
        throw Exception("unable to open the access block");

      // if the index starts with 0, include the owner by creating
      // a record for him.
      if (_index == 0)
        {
          // add the record to the range.
          range.insert(
            std::shared_ptr<nucleus::neutron::Record>{
              new nucleus::neutron::Record{context.object->owner_record()}});

          _index++;
        }

      // Seek the access responsible for the given index.
      auto _size = context.access_porcupine->size();

      // Decrement index because an index of 1 is actually 0 relative to the
      // porcupine because the record index-0 is the owner's.
      _index--;

      while (_size > 0)
        {
          auto pair = context.access_porcupine->seek(_index);
          auto& door = pair.first;
          auto& base = pair.second;

          door.open();

          auto start = _index - base;
          auto length = _size > (door().size() - start) ?
            (door().size() - start) : _size;

          ELLE_ASSERT(length != 0);

          // Retrieve the records falling in the requested
          // range [index, index + size[.
          nucleus::neutron::Range<nucleus::neutron::Record> subrange{
            door().consult(start, length)};

          door.close();

          // Inject the retrieved records into the main range.
          range.add(subrange);

          // Update the variables _index and _size.
          _index += length;
          _size -= length;
        }

      return elle::Status::Ok;
    }

    ///
    /// this method revokes a subject's access by updating the access records.
    ///
    elle::Status        Access::Revoke(
                          gear::Object&                         context,
                          const nucleus::neutron::Subject& subject)
    {
      ELLE_TRACE_FUNCTION(context, subject);

      // determine the rights over the object.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // verify that the user can modify the accesses.
      if (context.rights.role != nucleus::neutron::Object::RoleOwner)
        throw Exception("the user does not seem to have the permission to revoke "
               "access permissions");

      // update the access block or object according to the subject.
      if (subject == context.object->owner_subject())
        {
          //
          // in this case, the subject represents the object's owner.
          //

          // update the permissions.
          if (context.object->Administrate(
                context.object->attributes(),
                nucleus::neutron::permissions::none) == elle::Status::Error)
            throw Exception("unable to administrate the object");
        }
      else
        {
          //
          // otherwise, the subject is a lord being a user or a group.
          //

          // open the access.
          if (Access::Open(context) == elle::Status::Error)
            throw Exception("unable to open the access block");

          // Retrieve a door on the access.
          nucleus::proton::Door<nucleus::neutron::Access> door{
            context.access_porcupine->lookup(subject)};

          door.open();

          // remove the record.
          door().erase(subject);

          door.close();

          // Update the porcupine.
          context.access_porcupine->update(subject);

          // the object must be marked as administered i.e dirty so
          // that the meta signature gets re-computed i.e the access
          // fingerprint has probably changed.
          //
          // for more information, please refer to the Object class.
          if (context.object->Administrate(
                context.object->attributes(),
                context.object->owner_permissions()) == elle::Status::Error)
            throw Exception("unable to administrate the object");
        }

      // try to audit the object because the current author may have
      // lost its write permission in the process.
      if (Access::Audit(context, subject) == elle::Status::Error)
        throw Exception("unable to audit the object");

      // is the target subject the user i.e the object owner in this case.
      if (context.etoile().user_subject() == subject)
        {
          // update the context rights.
          if (Rights::Update(context,
                             nucleus::neutron::permissions::none) == elle::Status::Error)
            throw Exception("unable to update the rigths");
        }

      // set the context's state.
      context.state = gear::Context::StateModified;

      return elle::Status::Ok;
    }

    ///
    /// this method is called whenever an object's content has been modified
    /// leading to the generation of a new key.
    ///
    /// this key must then be distributed to every subject authorised to
    /// access the object in reading.
    ///
    /// for that purpose, tokens are generated.
    ///
    elle::Status        Access::Upgrade(
                          gear::Object&                         context,
                          cryptography::SecretKey const&                key)
    {
      ELLE_TRACE_FUNCTION(context, key);

      // open the access.
      if (Access::Open(context) == elle::Status::Error)
        throw Exception("unable to open the access");

      // XXX[all this loop was originally part of the Access class. it has
      //     however been extracted because, in order to encrypt the key
      //     for a group, it is necessary to load the group block.
      //     unfortunately, nucleus has no notion of loading/unloading blocks.
      //     one way would have been to provide a function pointer. for example
      //     a Depot class could be introduced in nucleus with virtual methods
      //     for pulling/pushing/wiping blocks.
      //     likewise for Downgrade() below, the loop was in a method in
      //     nucleus::neutron::Access.]

      // Seek the access responsible for the given index.
      nucleus::proton::Capacity _index = 0;
      auto _size = context.access_porcupine->size();

      while (_size > 0)
        {
          auto pair = context.access_porcupine->seek(_index);
          auto& door = pair.first;
          auto const& door_const = pair.first;
          auto& base = pair.second;

          ELLE_ASSERT(_index == base);

          door.open();

          // Update the variables _index and _size.
          _index += door().size();
          _size -= door().size();

          for (auto& _pair: door_const())
            {
              auto& record = _pair.second;

              // Ignore records which relate to subjects which do not have
              // the read permission; these ones do not have a token.
              if ((record->permissions() & nucleus::neutron::permissions::read) !=
                  nucleus::neutron::permissions::read)
                continue;

              switch (record->subject().type())
                {
                case nucleus::neutron::Subject::TypeUser:
                  {
                    // If the subject is a user, encrypt the key with the
                    // user's public key so that she will be the only one
                    // capable of decrypting it.

                    door().update(record->subject(),
                                  nucleus::neutron::Token(key, record->subject().user()));

                    context.access_porcupine->update(record->subject());

                    break;
                  }
                case nucleus::neutron::Subject::TypeGroup:
                  {
                    // If the subject is a group, the key is encrypted with the
                    // group's public pass. This way, the group members will be
                    // able to decrypt it since they have been distributed the
                    // private pass.

                    std::unique_ptr<nucleus::neutron::Group> group;

                    // XXX[the context should make use of unique_ptr instead
                    //     of releasing here.]
                    group =
                      context.etoile().depot().pull_group(
                        record->subject().group(),
                        nucleus::proton::Revision::Last);

                    door().update(record->subject(),
                                  nucleus::neutron::Token(key, group->pass_K()));

                    context.access_porcupine->update(record->subject());

                    break;
                  }
                default:
                  {
                    throw Exception("the access block contains unknown entries");
                  }
                }

              door.close();
            }
        }

      // then, create a new object's owner token.
      //
      // noteworthy is that the owner's token is always computed even though
      // the owner may not have the permission to read. this is required if the
      // owner wants to grant herself back or anyone else the permission
      // to read.
      nucleus::neutron::Token token(key, context.object->owner_K());

      // update the object with the new owner token.
      //
      // let us recall that the owner token is actually included in the
      // object's data section in order for authors to be able to re-generate
      // it.
      if (context.object->Update(
            context.object->author(),
            context.object->contents(),
            context.object->size(),
            context.object->access(),
            token) == elle::Status::Error)
        throw Exception("unable to update the object");

      // determine the rights over the object.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // finally, if the user has the permission to read, update its rights.
      if ((context.rights.permissions & nucleus::neutron::permissions::read) ==
          nucleus::neutron::permissions::read)
        {
          // recompute the rights with the new key.
          if (Rights::Recompute(context) == elle::Status::Error)
            throw Exception("unable to recompute the rights");
        }

      // set the context's state.
      context.state = gear::Context::StateModified;

      return elle::Status::Ok;
    }

    ///
    /// this method downgrades the access tokens i.e in opposition to
    /// upgrading with recompute them.
    ///
    /// this method is called whenever the object references no content.
    /// in such a case, no key needs to be distributed to the lords.
    ///
    /// therefore, the tokens must be reinitialized as null.
    ///
    elle::Status        Access::Downgrade(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      // open the access.
      if (Access::Open(context) == elle::Status::Error)
        throw Exception("unable to open the access");

      ELLE_TRACE("set the Access #%s records with a null token",
                 context.access_porcupine->size());

      // Seek the access responsible for the given index.
      nucleus::proton::Capacity _index = 0;
      auto _size = context.access_porcupine->size();

      while (_size > 0)
        {
          auto pair = context.access_porcupine->seek(_index);
          auto& door = pair.first;
          auto const& door_const = pair.first;
          auto& base = pair.second;

          ELLE_ASSERT(_index == base);

          door.open();

          // Update the variables _index and _size.
          _index += door().size();
          _size -= door().size();

          for (auto& _pair: door_const())
            {
              auto& record = _pair.second;

              // Check if the subject has the proper permissions.
              if ((record->permissions() & nucleus::neutron::permissions::read) !=
                  nucleus::neutron::permissions::read)
                continue;

              // Reset the token.
              door().update(record->subject(),
                            nucleus::neutron::Token::null());

              context.access_porcupine->update(record->subject());
            }

          door.close();
        }

      ELLE_TRACE("update the Object's owner token to null");

      // also update the owner's token.
      if (context.object->Update(
            context.object->author(),
            context.object->contents(),
            context.object->size(),
            context.object->access(),
            nucleus::neutron::Token::null()) == elle::Status::Error)
        throw Exception("unable to update the object");

      // determine the rights over the object.
      if (Rights::Determine(context) == elle::Status::Error)
        throw Exception("unable to determine the rights");

      // finally, if the user has the permission to read, update its rights.
      if ((context.rights.permissions & nucleus::neutron::permissions::read) ==
          nucleus::neutron::permissions::read)
        {
          // recompute the rights.
          if (Rights::Recompute(context) == elle::Status::Error)
            throw Exception("unable to recompute the rights");
        }

      // set the context's state.
      context.state = gear::Context::StateModified;

      return elle::Status::Ok;
    }

    ///
    /// this method destroys the access block by recording it in the
    /// transcript.
    ///
    elle::Status        Access::Destroy(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      // If the object holds some records, mark the blocks as
      // needing removal.
      if (context.object->access().empty() == false)
        {
          // Optimisation: only proceed if the access strategy is block-based:
          switch (context.object->access().strategy())
            {
            case nucleus::proton::Strategy::none:
              throw Exception("unable to destroy an empty content");
            case nucleus::proton::Strategy::value:
              {
                // Nothing to do in this case since there is no block for
                // holding the access.
                //
                // The optimization makes us save some time since the content
                // is not deserialized.

                break;
              }
            case nucleus::proton::Strategy::block:
            case nucleus::proton::Strategy::tree:
              {
                Access::Open(context);

                ELLE_TRACE("record the access blocks for removal");
                ELLE_ASSERT(context.access_porcupine != nullptr);
                context.access_porcupine->destroy();

                ELLE_TRACE("mark the destroyed blocks as needed to be "
                           "removed from the storage layer");
                context.transcript().merge(
                  context.access_nest->transcribe());

                break;
              }
            default:
              throw Exception
                (elle::sprintf("unknown strategy '%s'",
                               context.object->access().strategy()));
            }
        }

      return elle::Status::Ok;
    }

    ///
    /// this method closes the access.
    ///
    /// should have the access block been modified, the object is updated
    /// accordingly and the necessary blocks are recorded for storing.
    ///
    /// otherwise, nothing has to be done.
    ///
    elle::Status        Access::Close(
                          gear::Object&                         context)
    {
      ELLE_TRACE_FUNCTION(context);

      //
      // first, check if the block has been modified i.e exists and is dirty.
      //
      {
        // if there is no loaded access, then there is nothing to do.
        if (context.access_porcupine == nullptr)
          return elle::Status::Ok;

        // if the access has not changed, do nothing.
        if (context.access_porcupine->state() == nucleus::proton::State::clean)
          return elle::Status::Ok;
      }

      ELLE_TRACE("the Access block seems to have been modified");

      // retrieve the access's size.
      nucleus::neutron::Size size = context.access_porcupine->size();

      //
      // at this point, this access block is known to have been modified.
      //
      // modify the object according to the content of the access block.
      //

      // update the object according to the content.
      if (size == 0)
        {
          //
          // if the access became empty after removals, the
          // object should no longer point to any access block while
          // the old block should be deleted.
          //
          // however, since the object benefits from history i.e several
          // revisions, removing the access block would render the previous
          // revision inconsistent.
          //
          // therefore, the object is updated with a null access address.
          //
          // however, should the history functionality not be supported
          // by the network, the access block is marked for deletion.
          //

          ELLE_TRACE("the Access block is empty");

          // XXX: restore history handling
          // does the network support the history?
          // if (depot::hole().descriptor().meta().history() == false)
            {
              // destroy the access block.
              if (Access::Destroy(context) == elle::Status::Error)
                throw Exception("unable to destroy the access block");
            }

          // update the object with the null access address.
          if (context.object->Update(
                context.object->author(),
                context.object->contents(),
                context.object->size(),
                nucleus::proton::Radix{},
                context.object->owner_token()) == elle::Status::Error)
            throw Exception("unable to update the object");
        }
      else
        {
          //
          // otherwise, compute the address of the new access block and
          // update the object accordingly.
          //
          // note that the previous access block is not removed since
          // objects benefit from the history i.e multiple revisions; unless
          // the history support is not activated for this network.
          //
          ELLE_TRACE("the Access block is _not_ empty");

          // XXX: restore history handling
          // does the network support the history?
          // if (depot::hole().descriptor().meta().history() == false)
          /* XXX[porcupine: now the ancient blocks are not removed but
                 replaced and everithing is handled by porcupine/nest]
            {
              // destroy the access block.
              if (Access::Destroy(context) == elle::Status::Error)
                throw Exception("unable to destroy the access block");
            }
          */

          // XXX
          static cryptography::SecretKey secret_key(cryptography::cipher::Algorithm::aes256,
                                                    ACCESS_SECRET_KEY);

          // finally, update the object with the new access address.
          if (context.object->Update(
                context.object->author(),
                context.object->contents(),
                context.object->size(),
                context.access_porcupine->seal(secret_key),
                context.object->owner_token()) == elle::Status::Error)
            throw Exception("unable to update the object");

          // XXX[too slow without a nest optimization: to activate later]
          ELLE_STATEMENT(context.access_porcupine->check(nucleus::proton::flags::all));

          // mark the new/modified blocks as needing to be stored.
          context.transcript().merge(context.access_nest->transcribe());
        }

      return elle::Status::Ok;
    }

    ///
    /// this method checks if the current object's author is a lord or vassal.
    ///
    /// should it be the case, the system would have to check that the author
    /// did not lose his write permission during the last access control
    /// operation.
    ///
    /// should this occur, the future object retrieval would inevitably
    /// lead the client or server to detect the block as invalid since
    /// the author seems not to have had the right to modify the object.
    ///
    /// note that the _subject_ argument indicates the subject which
    /// access permissions have been altered, the system having to check
    /// that the new subject's permissions do not render the object
    /// inconsistent.
    ///
    elle::Status        Access::Audit(gear::Object&             context,
                                      const nucleus::neutron::Subject& subject)
    {
      ELLE_TRACE_FUNCTION(context, subject);

      // depending on the current author's role.
      switch (context.object->author().role)
        {
        case nucleus::neutron::Object::RoleOwner:
          {
            //
            // nothing to do in this case: the owner is changing the
            // access control permissions but he is also the one having
            // performed the latest modification on the object i.e the
            // current author.
            //

            break;
          }
        case nucleus::neutron::Object::RoleLord:
          {
            // XXX[cf neutron::Object]
            ELLE_ASSERT(false);
            /* XXX
            //
            // in this case however, the author is a lord.
            //
            // therefore, the system must make sure the access control
            // operation being performed is not removing the author's right
            // to write the object.
            //
            // to do that, the system tries to locate the access record
            // associated with the subject and verifies that he still has
            // the write permission.

            // open the access block.
            if (Access::Open(context) == elle::Status::Error)
              throw Exception("unable to open the access block");

            // check whether a record exist for the subject as it
            // could very well have been removed.
            if (context.access->exist(subject) == true)
              {
                nucleus::neutron::Record const& record =
                  context.access->locate(subject);

                // check that the subject, also author, still has the
                // write permission.
                //
                // if he has, nothing has to be done.
                if ((record.permissions() &
                     nucleus::neutron::permissions::write) ==
                    nucleus::neutron::permissions::write)
                  break;
              }

            // this point is reached if the subject no longer has the
            // write permission, in which case the object's access
            // control mechanism is regulated.
            if (Access::Regulate(context) == elle::Status::Error)
              throw Exception("unable to regulate the object");
            */

            break;
          }
        case nucleus::neutron::Object::RoleVassal:
          {
            // XXX to implement.
            assert(false && "not supported yet");

            break;
          }
        case nucleus::neutron::Object::RoleUnknown:
        case nucleus::neutron::Object::RoleNone:
        default:
          {
            throw Exception(elle::sprintf("invalid role '%u'",
                                                context.object->author().role));
          }
        }

      return elle::Status::Ok;
    }

    ///
    /// this method is called whenever the object needs to be regulated
    /// i.e the current author has to be re-generated because the author
    /// just lost the permission to write the object.
    ///
    /// should this occur, the object owner would re-sign the data to
    /// make sure the object consistency is ensured.
    ///
    elle::Status        Access::Regulate(gear::Object&          context)
    {
      ELLE_TRACE_FUNCTION(context);

      // update the object with a new author. since the object gets updated,
      // it will be re-signed during the object's sealing process.
      if (context.object->Update(
            nucleus::neutron::Author{},
            context.object->contents(),
            context.object->size(),
            context.object->access(),
            context.object->owner_token()) == elle::Status::Error)
        throw Exception("unable to update the object");

      return elle::Status::Ok;
    }

    cryptography::Digest
    Access::fingerprint(gear::Object& context)
    {
      ELLE_TRACE_FUNCTION(context);

      Access::Open(context);

      return (nucleus::neutron::access::fingerprint(*context.access_porcupine));
    }
  }
}
