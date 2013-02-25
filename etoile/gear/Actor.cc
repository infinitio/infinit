#include <etoile/gear/Actor.hh>
#include <etoile/gear/Scope.hh>

namespace etoile
{
  namespace gear
  {

//
// ---------- definitions -----------------------------------------------------
//

    ///
    /// this data structure holds all the living actors.
    ///
    Actor::Container            Actor::Actors;

//
// ---------- static methods --------------------------------------------------
//

    ///
    /// this method initializes the actor system.
    ///
    elle::Status        Actor::Initialize()
    {
      return elle::Status::Ok;
    }

    ///
    /// this method cleans the actor system.
    ///
    elle::Status        Actor::Clean()
    {
      Actor::Scoutor    scoutor;

      // go through the container.
      for (scoutor = Actor::Actors.begin();
           scoutor != Actor::Actors.end();
           scoutor++)
        {
          Actor*                actor = scoutor->second;

          // delete the actor: this action will detach the actor from
          // the scope.
          delete actor;
        }

      // clear the container.
      Actor::Actors.clear();

      return elle::Status::Ok;
    }

    ///
    /// this method retrieves an actor according to its identifier.
    ///
    elle::Status        Actor::Add(const Identifier&            identifier,
                                   Actor*                       actor)
    {
      std::pair<Actor::Iterator, elle::Boolean> result;


      // check if this identifier has already been recorded.
      if (Actor::Actors.find(identifier) != Actor::Actors.end())
        throw elle::Exception("this actor seems to have already been registered");

      // insert the actor in the container.
      result = Actor::Actors.insert(
                 std::pair<const Identifier, Actor*>(identifier, actor));

      // check the result.
      if (result.second == false)
        throw elle::Exception("unable to insert the actor in the container");

      return elle::Status::Ok;
    }

    ///
    /// this method returns the actor associated with the given identifier.
    ///
    elle::Status        Actor::Select(const Identifier&         identifier,
                                      Actor*&                   actor)
    {
      Actor::Scoutor    scoutor;


      // find the entry.
      if ((scoutor = Actor::Actors.find(identifier)) == Actor::Actors.end())
        throw elle::Exception("unable to locate the actor associated with the identifier");

      // return the actor.
      actor = scoutor->second;

      return elle::Status::Ok;
    }

    ///
    /// this method removes an actor from the container.
    ///
    elle::Status        Actor::Remove(const Identifier&         identifier)
    {
      Actor::Iterator   iterator;


      // find the entry.
      if ((iterator = Actor::Actors.find(identifier)) == Actor::Actors.end())
        throw elle::Exception("unable to locate the actor associated with the identifier");

      // erase the entry.
      Actor::Actors.erase(iterator);

      return elle::Status::Ok;
    }

    ///
    /// this method displays the actors data structure.
    ///
    elle::Status        Actor::Show(const elle::Natural32       margin)
    {
      elle::String      alignment(margin, ' ');
      Actor::Scoutor    scoutor;


      std::cout << alignment << "[Actor]" << std::endl;

      // go through the container.
      for (scoutor = Actor::Actors.begin();
           scoutor != Actor::Actors.end();
           scoutor++)
        {
          // dump the identifier.
          if (scoutor->first.Dump(margin + 2) == elle::Status::Error)
            throw elle::Exception("unable to dump the identifier");

          // dump the actor.
          if (scoutor->second->Dump(margin + 2) == elle::Status::Error)
            throw elle::Exception("unable to dump the actor");

        }

      return elle::Status::Ok;
    }

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor.
    ///
    Actor::Actor(Scope*                                         scope):
      scope(scope),
      state(Actor::StateClean)
    {
      // generate an identifier.
      if (this->identifier.Generate() == elle::Status::Error)
        return;

      // register the actor.
      if (Actor::Add(this->identifier, this) == elle::Status::Error)
        return;

      // add the actor to the scope's set.
      if (scope->Attach(this) == elle::Status::Error)
        return;
    }

    ///
    /// destructor.
    ///
    Actor::~Actor()
    {
      // remove the actor from the scope's set.
      if (this->scope->Detach(this) == elle::Status::Error)
        return;

      // unregister the actor.
      if (Actor::Remove(this->identifier) == elle::Status::Error)
        return;
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method is called to indicate the operation being performed
    /// by the actor.
    ///
    /// let us recall that multiple actors operate on the same scope.
    ///
    /// therefore, since modifications are directly applied on the context
    /// when requested, an actor cannot perform modifications and finally
    /// decide to discard them.
    ///
    /// this method therefore checks that the operation is consistent
    /// regarding the previous requests i.e the actor's state.
    ///
    elle::Status        Actor::Operate(const Operation          operation)
    {
      // process the operation.
      switch (operation)
        {
        case OperationDiscard:
          {
            //
            // the actor is discarding the context.
            //
            // thus, the actor must never have performed a modification
            // on the context.
            //
            // note that there is a catch: if the actor has modified a
            // freshly created scope, it can actually be discarded. this
            // is made possible because (i) it is a common thing for
            // an application to create an object and finally realize it
            // does not have the permission to add it to a directory for
            // instance (ii) a created object cannot be accessed i.e loaded
            // by another actor since it is not referenced yet by a chemin.

            // check the scope's nature i.e does it have a chemin.
            if (this->scope->chemin != path::Chemin::Null)
              {
                //
                // the normal case: check that no modification has been
                // performed unless it is alone.
                //

                // check the state, especially if there are multiple actors.
                if (this->scope->actors.size() == 1)
                  {
                    // Nothing to do: we assume the actor can exceptionally
                    // modify an object and discard his modifications if
                    // he is alone because this is equivalent to the user
                    // re-modifying the object the other way around, ending
                    // up with the exact same original state.
                  }
                else
                  {
                    if (this->state != Actor::StateClean)
                      throw elle::Exception("unable to discard previously performed "
                             "modifications");
                  }
              }
            else
              {
                //
                // the exceptional case: allow the actor to discard a
                // created scope.
                //

                // do nothing.
              }

            break;
          }
        case OperationStore:
          {
            //
            // the actor is storing the potentially modified context.
            //
            // there is nothing to do here: should the actor have updated
            // the context or not, the store operation can be requested.
            //

            break;
          }
        case OperationDestroy:
          {
            //
            // the actor is destroying the context, even though it
            // has been modified.
            //
            // as for the store operation, this operation is valid
            // no matter the actor's state.
            //

            break;
          }
        case OperationUnknown:
          {
            throw elle::Exception
              (elle::sprintf("unable to process the closing operation '%u'\n",
                             operation));
          }
        }

      return elle::Status::Ok;
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps an actor.
    ///
    elle::Status        Actor::Dump(const elle::Natural32       margin) const
    {
      elle::String      alignment(margin, ' ');

      std::cout << alignment << "[Actor]" << std::endl;

      // dump the identifier.
      if (this->identifier.Dump(margin + 2) == elle::Status::Error)
        throw elle::Exception("unable to dump the identifier");

      // dump the scope's address.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Scope] " << std::hex << this->scope << std::endl;

      // dump the state.
      std::cout << alignment << elle::io::Dumpable::Shift
                << "[State] " << std::dec << this->state << std::endl;

      return elle::Status::Ok;
    }

  }
}
