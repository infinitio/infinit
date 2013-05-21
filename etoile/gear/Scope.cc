#include <elle/Exception.hh>

#include <etoile/gear/Scope.hh>
#include <etoile/gear/Directory.hh>
#include <etoile/gear/File.hh>
#include <etoile/gear/Link.hh>
#include <etoile/gear/Object.hh>
#include <etoile/gear/Group.hh>
#include <etoile/gear/Chronicle.hh>
#include <etoile/Exception.hh>

#include <elle/log.hh>

#include <Infinit.hh>
#include <Scheduler.hh>

namespace etoile
{
  namespace gear
  {

    ELLE_LOG_COMPONENT("infinit.etoile.gear.Scope");

//
// ---------- definitions -----------------------------------------------------
//

    ///
    /// this container holds the scopes accessible through a chemin.
    ///
    Scope::S::O::Container              Scope::Scopes::Onymous;

    ///
    /// this container holds the anonymous---i.e freshly author---scopes.
    ///
    Scope::S::A::Container              Scope::Scopes::Anonymous;

//
// ---------- static methods --------------------------------------------------
//

    ///
    /// this method initializes the scope system.
    ///
    elle::Status        Scope::Initialize()
    {
      // nothing to do.

      return elle::Status::Ok;
    }

    ///
    /// this method cleans the scope system.
    ///
    elle::Status        Scope::Clean()
    {
      //
      // release the onymous scopes.
      //
      {
        Scope::S::O::Scoutor    scoutor;

        // go through the container.
        for (scoutor = Scope::Scopes::Onymous.begin();
             scoutor != Scope::Scopes::Onymous.end();
             scoutor++)
          {
            Scope*              scope = scoutor->second;

            // delete the scope.
            delete scope;
          }

        // clear the container.
        Scope::Scopes::Onymous.clear();
      }

      //
      // release the anonymous scopes.
      //
      {
        Scope::S::A::Scoutor    scoutor;

        // go through the container.
        for (scoutor = Scope::Scopes::Anonymous.begin();
             scoutor != Scope::Scopes::Anonymous.end();
             scoutor++)
          {
            Scope*              scope = *scoutor;

            // delete the scope.
            delete scope;
          }

        // clear the container.
        Scope::Scopes::Anonymous.clear();
      }

      return elle::Status::Ok;
    }

    ///
    /// this method returns true if an anymous scope exists for
    /// the given chemin.
    ///
    elle::Boolean       Scope::Exist(const path::Chemin&        chemin)
    {
      Scope::S::O::Scoutor      scoutor;

      if ((scoutor = Scope::Scopes::Onymous.find(chemin)) !=
          Scope::Scopes::Onymous.end())
        return true;

      return false;
    }

    ///
    /// this method inserts an anymous scope.
    ///
    elle::Status        Scope::Add(const path::Chemin&          chemin,
                                   Scope*                       scope)
    {
      std::pair<Scope::S::O::Iterator, elle::Boolean>   result;

      // insert the scope in the container.
      result = Scope::Scopes::Onymous.insert(
        std::pair<const path::Chemin, Scope*>(chemin, scope));

      // check the result.
      if (result.second == false)
        throw Exception("unable to insert the scope in the container");

      return elle::Status::Ok;
    }

    ///
    /// this method returns the anymous scope associated with the given chemin.
    ///
    elle::Status        Scope::Retrieve(const path::Chemin&     chemin,
                                        Scope*&                 scope)
    {
      Scope::S::O::Scoutor      scoutor;

      if ((scoutor = Scope::Scopes::Onymous.find(chemin)) ==
          Scope::Scopes::Onymous.end())
        throw Exception("unable to locate the scope associated with the given chemin");

      scope = scoutor->second;

      return elle::Status::Ok;
    }

    ///
    /// this method removes the onymous scope associated with the given
    /// chemin.
    ///
    elle::Status        Scope::Remove(const path::Chemin&       chemin)
    {
      Scope::S::O::Iterator     iterator;

      if ((iterator = Scope::Scopes::Onymous.find(chemin)) ==
          Scope::Scopes::Onymous.end())
        throw Exception("unable to locate the scope associated with the given chemin");

      Scope::Scopes::Onymous.erase(iterator);

      return elle::Status::Ok;
    }

    ///
    /// this method inserts an anonymous scope.
    ///
    elle::Status        Scope::Add(Scope*                       scope)
    {
      // insert the scope in the anonymous container.
      Scope::Scopes::Anonymous.push_back(scope);

      return elle::Status::Ok;
    }

    ///
    /// this method removes an anonymous scope.
    ///
    elle::Status        Scope::Remove(Scope*                    scope)
    {
      Scope::Scopes::Anonymous.remove(scope);

      return elle::Status::Ok;
    }

    ///
    /// this method inserts the given scope in the appropriate container,
    /// i.e according to its chemin.
    ///
    elle::Status        Scope::Inclose(Scope*                   scope)
    {
      // depending on the scope i.e its chemin.
      if (scope->chemin == path::Chemin::Null)
        {
          if (Scope::Add(scope) == elle::Status::Error)
            throw Exception("unable to add the anonymous scope");
        }
      else
        {
          if (Scope::Add(scope->chemin, scope) == elle::Status::Error)
            throw Exception("unable to add the onymous scope");
        }

      return elle::Status::Ok;
    }

    ///
    /// this method tries to locate an existing scope given the chemin.
    ///
    /// if none exists, a scope is created and added to the container.
    ///
    elle::Status        Scope::Acquire(const path::Chemin&      chemin,
                                       Scope*&                  scope)
    {
      if (Scope::Exist(chemin) == false)
        {
          // allocate a new scope.
          auto s = std::unique_ptr<Scope>(new Scope(chemin));

          // create the scope.
          if (s->Create() == elle::Status::Error)
            throw Exception("unable to create the scope");

          // inclose the scope.
          if (Scope::Inclose(s.get()) == elle::Status::Error)
            throw Exception("unable to inclose the scope");

          // return the scope.
          scope = s.release();
        }
      else
        {
          // retrieve the existing scope.
          if (Scope::Retrieve(chemin, scope) == elle::Status::Error)
            throw Exception("unable to retrieve the existing scope");
        }

      return elle::Status::Ok;
    }

    ///
    /// this specific-revision of the method creates a scope without
    /// looking for an existing one.
    ///
    elle::Status        Scope::Supply(Scope*&                   scope)
    {
      // allocate a new scope.
      auto s = std::unique_ptr<Scope>(new Scope);

      // create the scope.
      if (s->Create() == elle::Status::Error)
        throw Exception("unable to create the scope");

      // inclose the scope.
      if (Scope::Inclose(s.get()) == elle::Status::Error)
        throw Exception("unable to inclose the scope");

      // return the scope.
      scope = s.release();

      return elle::Status::Ok;
    }

    ///
    /// this method removes the scope from its container making the scope
    /// orphan.
    ///
    /// therefore, it is the responsibility of the caller to delete the scope.
    ///
    elle::Status        Scope::Relinquish(Scope*                scope)
    {
      // depending on the scope type.
      if (scope->chemin == path::Chemin::Null)
        {
          //
          // in this case the scope is anonymous.
          //

          if (Scope::Remove(scope) == elle::Status::Error)
            throw Exception("unable to remove the anonymous scope");
        }
      else
        {
          //
          // in this case, the scope is onymous.
          //

          if (Scope::Remove(scope->chemin) == elle::Status::Error)
            throw Exception("unable to remove the onymous scope");
        }

      return elle::Status::Ok;
    }

    ///
    /// should the given scope become unused---i.e no more actor is
    /// operating on it---, it is relinquished and deleted.
    ///
    elle::Status        Scope::Annihilate(Scope*                scope)
    {

      // if no actor operates on it anymore.
      if (scope->actors.empty() == true)
        {
          // relinquish the scope.
          if (Scope::Relinquish(scope) == elle::Status::Error)
            throw Exception("unable to relinquish the scope");

          // and finally, delete it.
          delete scope;
        }

      return elle::Status::Ok;
    }

    ///
    /// update the scope container which may contain scopes
    /// referencing invalid chemins.
    ///
    /// XXX note that the mechanism below is not very efficient.
    ///     instead a tree-based data structure should be used
    ///     in order to update the container in an efficient
    ///     manner.
    ///
    elle::Status        Scope::Update(const path::Chemin&       from,
                                      const path::Chemin&       to)
    {
      //
      // go through the onymous scopes and update the chemins
      // should them derive _from_.
      //

    retry:
      auto              iterator = Scope::Scopes::Onymous.begin();
      auto              end = Scope::Scopes::Onymous.end();

      for (; iterator != end; ++iterator)
        {
          Scope*        scope = iterator->second;

          if (scope->chemin.Derives(from) == false)
            continue;

          //
          // the scope's chemin seems to derive the base
          // i.e _from_.
          //
          // it must therefore be updated so as to be
          // consistent.
          //

          scope->chemin = to;

          //
          // note that the current scope is registered in
          // the container with its old chemin as the key.
          //
          // therefore, the container's key for this scope
          // must also be updated.
          //
          // the following thus removes the scope and
          // re-inserts it.
          //

          if (Scope::Remove(from) == elle::Status::Error)
            throw Exception("unable to remove the scope");

          if (Scope::Add(scope->chemin, scope) == elle::Status::Error)
            {
              //
              // in this extreme case, manually delete the scope
              // which is now orphan.
              //

              delete scope;

              throw Exception("unable to re-insert the scope");
            }

          //
          // at this point, we cannot go further with the
          // iterator as consistency cannot be guaranteed
          // anymore.
          //
          // therefore, go through the whole process again.
          //

          goto retry;
        }

      return (elle::Status::Ok);
    }

    ///
    /// this method displays the containers.
    ///
    elle::Status        Scope::Show(const elle::Natural32       margin)
    {
      elle::String      alignment(margin, ' ');


      std::cout << alignment << "[Scope]" << std::endl;

      //
      // onymous scopes.
      //
      {
        Scope::S::O::Scoutor    scoutor;

        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Onymous] " << Scope::Scopes::Onymous.size()
                  << std::endl;

        // go through the onymous scopes.
        for (scoutor = Scope::Scopes::Onymous.begin();
             scoutor != Scope::Scopes::Onymous.end();
             scoutor++)
          {
            // dump the scope, if present.
            if (scoutor->second == nullptr)
              {
                std::cout << alignment << elle::io::Dumpable::Shift
                          << elle::io::Dumpable::Shift
                          << "[Scope] " << "none" << std::endl;
              }
            else
              {
                if (scoutor->second->Dump(margin + 4) == elle::Status::Error)
                  throw Exception("unable to dump the scope");
              }
          }
      }

      //
      // anonymous scopes.
      //
      {
        Scope::S::A::Scoutor    scoutor;

        std::cout << alignment << elle::io::Dumpable::Shift
                  << "[Anonymous] " << Scope::Scopes::Anonymous.size()
                  << std::endl;

        // go through the anonymous scopes.
        for (scoutor = Scope::Scopes::Anonymous.begin();
             scoutor != Scope::Scopes::Anonymous.end();
             scoutor++)
          {
            // dump the scope, if present.
            if (*scoutor == nullptr)
              {
                std::cout << alignment << elle::io::Dumpable::Shift
                          << elle::io::Dumpable::Shift
                          << "[Scope] " << "none" << std::endl;
              }
            else
              {
                if ((*scoutor)->Dump(margin + 4) == elle::Status::Error)
                  throw Exception("unable to dump the scope");
              }
          }
      }

      return elle::Status::Ok;
    }

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor especially useful for anonymous scopes.
    ///
    Scope::Scope():
      state(StateNone),
      context(nullptr),
      chronicle(nullptr)
    {
    }

    ///
    /// chemin-specific constructor.
    ///
    Scope::Scope(const path::Chemin&                            chemin):
      state(StateNone),
      chemin(chemin),
      context(nullptr),
      chronicle(nullptr)
    {
    }

    ///
    /// destructor.
    ///
    Scope::~Scope()
    {
      Scope::A::Scoutor scoutor;

      // delete the context.
      if (this->context != nullptr)
        delete this->context;

      // delete the chronicle.
      if (this->chronicle != nullptr)
        delete this->chronicle;

      // release the actors, if some remain.
      for (scoutor = this->actors.begin();
           scoutor != this->actors.end();
           scoutor++)
        {
          Actor*        actor = *scoutor;

          // delete the actor.
          delete actor;
        }

      // clear the actors container.
      this->actors.clear();
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method creates a scope.
    ///
    elle::Status        Scope::Create()
    {
      return elle::Status::Ok;
    }

    ///
    /// this method adds an actor to the scope's set of actors.
    ///
    elle::Status        Scope::Attach(Actor*                    actor)
    {

      // try to locate an existing actor.
      if (this->Locate(actor) == true)
        throw Exception("this actor seems to have been already registered");

      // add the actor to the container.
      this->actors.push_back(actor);

      return elle::Status::Ok;
    }

    ///
    /// this method returns true if the actor has been located.
    ///
    elle::Boolean       Scope::Locate(Actor*                    actor,
                                      Scope::A::Iterator*       iterator)
    {
      Scope::A::Iterator        i;


      // go through the set of actors.
      for (i = this->actors.begin();
           i != this->actors.end();
           i++)
        {
          // try this actor.
          if (actor == *i)
            {
              // return the iterator if necessary.
              if (iterator != nullptr)
                *iterator = i;

              return true;
            }
        }

      return false;
    }

    ///
    /// this method remvoes an actor from the scope's set of actors.
    ///
    elle::Status        Scope::Detach(Actor*                    actor)
    {
      Scope::A::Iterator        iterator;


      // try to locate an existing actor.
      if (this->Locate(actor, &iterator) == false)
        throw Exception("no such actor seems to have been registered");

      // remove the actor.
      this->actors.erase(iterator);

      return elle::Status::Ok;
    }

    ///
    /// this method is called to indicate the operation being performed
    /// on the scope by the actor.
    ///
    /// note that since multiple actors operate on the same scope, one must
    /// assume that other actors may have modified or even destroy the
    /// scope's target.
    ///
    elle::Status        Scope::Operate(const Operation          operation)
    {

      // update the context's closing operation according to its given
      // value and the given operation.
      switch (operation)
        {
        case OperationDiscard:
          {
            // depending on the current context's closing operation.
            switch (this->context->operation)
              {
              case OperationUnknown:
                {
                  //
                  // in this case, the given closing operation is the first
                  // one to take place.
                  //
                  // thus, the context is marked as requiring to be discarded.
                  //

                  // set the context's closing operation.
                  this->context->operation = operation;

                  break;
                }
              case OperationDiscard:
                {
                  //
                  // the given closing operation is the same as the current
                  // context's.
                  //
                  // thus, everything seems fine this way.
                  //

                  break;
                }
              case OperationStore:
                {
                  //
                  // the context's modifications have been marked as requiring
                  // to be stored.
                  //
                  // therefore, the given operation does not imply any change
                  // of plan.
                  //

                  break;
                }
              case OperationDestroy:
                {
                  //
                  // as for the OperationStore, in this case, the context
                  // has been marked for deletion.
                  //
                  // therefore, the discarding given operation does not
                  // change the scope's closing operation i.e Destroy.
                  //

                  break;
                }
              }

            break;
          }
        case OperationStore:
          {
            // depending on the current context's closing operation.
            switch (this->context->operation)
              {
              case OperationUnknown:
                {
                  //
                  // in this case, the given closing operation is the first
                  // one to take place.
                  //
                  // thus, the context is marked as requiring to be stored.
                  //

                  // set the context's closing operation.
                  this->context->operation = operation;

                  break;
                }
              case OperationDiscard:
                {
                  //
                  // the given closing operation is of higher importance than
                  // the existing one.
                  //
                  // therefore, the closing operation is set to: Store.
                  //

                  // set the context's closing operation.
                  this->context->operation = operation;

                  break;
                }
              case OperationStore:
                {
                  //
                  // the context's modifications have been marked as requiring
                  // to be stored.
                  //
                  // since the given operation is identical, the context's
                  // closing operation does not need to be changed.
                  //

                  break;
                }
              case OperationDestroy:
                {
                  //
                  // in this case, the context has been marked for deletion.
                  //
                  // since the storing given operation is of lower importance,
                  // the closing operation is maintained.
                  //

                  break;
                }
              }

            break;
          }
        case OperationDestroy:
          {
            // depending on the current context's closing operation.
            switch (this->context->operation)
              {
              case OperationUnknown:
                {
                  //
                  // in this case, the given closing operation is the first
                  // one to take place.
                  //
                  // thus, the context is marked as requiring to be destroyed.
                  //

                  // set the context's closing operation.
                  this->context->operation = operation;

                  break;
                }
              case OperationDiscard:
                {
                  //
                  // the given closing operation is of higher importance than
                  // the existing one.
                  //
                  // therefore, the closing operation is set to: Destroy.
                  //

                  // set the context's closing operation.
                  this->context->operation = operation;

                  break;
                }
              case OperationStore:
                {
                  //
                  // in this case, although some actors may have modified
                  // the context---since the current closing operation is
                  // Store---, the operation is set to Destroy because the
                  // given operation superseeds the current one.
                  //

                  // set the context's closing operation.
                  this->context->operation = operation;

                  break;
                }
              case OperationDestroy:
                {
                  //
                  // in this case, the context has already been marked
                  // for deletion.
                  //
                  // therefore, the closing operation is maintained.
                  //

                  break;
                }
              }

            break;
          }
        case OperationUnknown:
          {
            throw Exception
              (elle::sprintf("unable to process the closing operation '%u'\n",
                             operation));
          }
        }

      return elle::Status::Ok;
    }

    template <typename T>
    elle::Status Scope::_shutdown()
    {
      T* context = static_cast<T*>(this->context);
      assert(dynamic_cast<T*>(this->context) != nullptr);

      // depending on the closing operation...
      switch (this->context->operation)
      {
      case OperationDiscard:
        // call the shutdown method.
        if (T::A::Discard(*context) == elle::Status::Error)
          throw Exception("an error occured in the shutdown method");
        break;
      case OperationStore:
        // call the shutdown method.
        if (T::A::Store(*context) == elle::Status::Error)
          throw Exception("an error occured in the shutdown method");
        break;
      case OperationDestroy:
        // call the shutdown method.
        if (T::A::Destroy(*context) == elle::Status::Error)
          throw Exception("an error occured in the shutdown method");
        break;
      case OperationUnknown:
      default:
        throw Exception(elle::sprintf("unknown operation '%u'\n",
                                            this->context->operation));
      }
      return elle::Status::Ok;
    }

    ///
    /// this method triggers the shutdown method whose role is to close
    /// a context it order for it to be recorded in the journal.
    ///
    /// note however that if actors are still working on the scope, this
    /// method does nothing as one the last actor triggers the actual
    /// closing operation.
    ///
    elle::Status Scope::Shutdown()
    {
      ELLE_TRACE_FUNCTION("");

      // if actors remain, do nothing.
      //
      // indeed, only the final actor will trigger the shutdown operation. this
      // way, potential conflicts are prevented while expensive cryptographic
      // operations are performed only once.
      if (this->actors.empty() == false)
      {
        ELLE_DEBUG("actors remain: ignore the shutdown");

        return elle::Status::Ok;
      }

      ELLE_DEBUG("no actors remaining");

      // relinquish the scope: at this point we know there is no
      // remaining actor.
      //
      // Note that this action was previously being performed in the
      // wall, right after releasing the lock on the scope and
      // before proceeding to recording the scope in the journal.
      //
      // Unfortunately, this caused a problem because of this possible
      // scenario given two threads T1 and T2:
      //
      //   1) T1 acquires the scope and starts the process of shutdown
      //   2) T1 yields because of a network operation
      //   3) T2 is scheduled, acquires the scope since still registered
      //   4) T2 tries to lock the scope but fails since T1 has acquired this
      //      lock and is still operating on the scope.
      //   5) T1 is rescheduled, continues the shutdown process and completes
      //      it by recording the scope in the journal and finally deleting
      //      the scope, assuming it was the only actor since shutting down
      //      the scope.
      //   6) T2 is scheduled, acquires the lock and tries to use the scope
      //      though it has actually been deleted by T1.
      //
      // The problem is solved by unregistering the scope as soon as we know
      // we are going to go through the shutdown process and that, from now
      // on, no more actors should be able to attach to it.
      Scope::Relinquish(this);

      //
      // otherwise, the current actor is the last one and is responsible
      // for triggering the shutdown operation.
      //
      // therefore, the appropriate closing operation is triggered
      // according to the nature of the scope's context. indeed, noteworthy
      // is that the actor calling this Shutdown() method may be
      // operating on, say an Object, although the original scope was
      // created by loading a File. these behaviours are perfectly
      // valid because File contexts derive Objects'. this method
      // must however trigger the closing operation on the original
      // context's type i.e File in this example.
      //

      try
        {
          // depending on the context's nature.
          switch (this->context->nature)
          {
          case NatureObject:
            return this->_shutdown<gear::Object>();
          case NatureFile:
            return this->_shutdown<gear::File>();
          case NatureDirectory:
            return this->_shutdown<gear::Directory>();
          case NatureLink:
            return this->_shutdown<gear::Link>();
          case NatureGroup:
            return this->_shutdown<gear::Group>();
          case NatureUnknown:
          default:
            throw Exception(elle::sprintf("unknown context nature '%u'",
                                                this->context->nature));
          }
        }
      catch (std::exception const& err)
        {
          if (this->actors.empty())
            {
              ELLE_DEBUG("destroy the scope %s", *this);
              if (Scope::Relinquish(this) == elle::Status::Error)
                throw Exception("unable to relinquish the scope");
            }
          std::string what{err.what()};
          throw Exception(elle::sprintf("the shutdown process failed: %s",
                                              what));
        }

      return elle::Status::Ok;
    }

    ///
    /// this method is called whenever the scope needs to be refreshed i.e
    /// has lived long enough in main memory so that the risk of it having
    /// been updated on another computer is quite high.
    ///
    /// therefore, this refreshing process is triggered on a regular basis
    /// in order to make sure scopes which are always opened pick get
    /// updated.
    ///
    template <typename T>
    elle::Status        Scope::Refresh()
    {
      // debug.
      if (Infinit::Configuration.etoile.debug == true)
        printf("[etoile] gear::Scope::Refresh()\n");

      // lock the current scope in order to make sure it does not get
      // relinquished or simply modified.
      //
      // this is especially required since Load()ing may block the current
      // fiber.
      reactor::Lock lock(infinit::scheduler(), mutex.write());
      {
        // allocate a context.
        auto context = std::unique_ptr<T>(new T);

        // locate the context based on the current scope's chemin.
        if (this->chemin.Locate(context->location) == elle::Status::Error)
          throw Exception("unable to locate the scope");

        // load the object.
        if (T::A::Load(*context) == elle::Status::Error)
          throw Exception("unable to load the object");

        // check if the loaded object is indeed newer.
        if (context->object->revision() >
            static_cast<T*>(this->context)->object->revision())
          {
            //
            // in this case, a newer revision exists which has been loaded.
            //
            // replace the current one with the new one.
            //

            // delete the existing context.
            delete this->context;

            // set the new context.
            this->context = context.release();
          }
        //
        // otherwise, the loaded object is of the same revision as the
        // current one.
        //
        // in this case, nothing is done.
        //
      }

      return elle::Status::Ok;
    }

    ///
    /// this method does the opposite of the Refresh() method by disclosing,
    /// i.e storing, the modifications even though the scope has not been
    /// closed yet.
    ///
    /// such a process gets handy when scopes are opened and never closed
    /// by still modified. thanks to the regular disclosing mechanism, the
    /// modifications of scopes having lives for too much time in main
    /// memory are published by force in order to make sure other computers
    /// take notice of those.
    ///
    template <typename T>
    elle::Status        Scope::Disclose()
    {
      // debug.
      if (Infinit::Configuration.etoile.debug == true)
        printf("[etoile] gear::Scope::Disclose()\n");

      reactor::Lock lock(infinit::scheduler(), mutex.write());
      {
        Scope*          scope = nullptr;
        T*              context = nullptr;
        Actor*          actor = nullptr;

        struct OnExit
        {
          Actor*   actor;
          Scope*   scope;
          bool      track;

          OnExit() :
            actor(nullptr), scope(nullptr), track(true)
          {}
          ~OnExit()
          {
            if (!this->track)
              return;
            delete this->actor;
            if (this->scope != nullptr)
              gear::Scope::Annihilate(this->scope);
          }
        } guard;

        //
        // create a scope, very much as for wall::*::Create(), except
        // that it works even for objects which cannot, obviously, be created.
        //
        // supply a scope i.e request a new anonymous scope.
        if (gear::Scope::Supply(scope) == elle::Status::Error)
          throw Exception("unable to supply a scope");
        guard.scope = scope;

        // retrieve the context.
        if (scope->Use(context) == elle::Status::Error)
          throw Exception("unable to retrieve the context");

        // allocate an actor on the new scope, making the scope valid
        // for triggering automata.
        actor = new gear::Actor(scope);
        guard.actor = actor;

        //
        // swap the contexts.
        //
        {
          // transfer the current scope's context to the new scope.
          actor->scope->context = this->context;

          // set the current scope's context with the new one.
          this->context = context;
        }

        // store the object which now carries the modified context.
        T::W::store(actor->identifier);

        // waive the actor and the scope
        guard.track = false;

        //
        // at this point, a scope has been created, to which the modified
        // context has been transferred. this scope has been stored, hence
        // disclosing its modifications.
        //
        // finally, the current scope's context is allocated but initialized
        // and must therefore be loaded with a fresh revision of the object.
        //

        // locate the object based on the current scope's chemin.
        if (this->chemin.Locate(context->location) == elle::Status::Error)
          throw Exception("unable to locate the file");

        // load a fresh revision of the object which should happen to be
        // the one stored above.
        if (T::A::Load(*context) == elle::Status::Error)
          throw Exception("unable to load the object");
      }

      return elle::Status::Ok;
    }

//
// ---------- callbacks -------------------------------------------------------
//

    ///
    /// this callback is triggered whenever the scope is considered having
    /// expired.
    ///
    /// if the scope has reached a certain lifetime and has not been modified,
    /// the supervisor refreshes it by fetching a potentially new revision of
    /// the object. this ensures that a scope being still in use because of
    /// an actor never releasing it, will, from time to time, be refreshed
    /// in order to provide the active actors the latest view.
    ///
    /// likewise, if the scope has reached a certain lifetime and has been
    /// modified, the supervisor forces the scope's modifications to be
    /// disclosed so that, although non-active actors remained attached
    /// to this scope, the network gets updated according to the modifications
    /// performed locally.
    ///
    elle::Status        Scope::Supervisor()
    {
      // debug.
      if (Infinit::Configuration.etoile.debug == true)
        printf("[etoile] gear::Scope::Supervisor()\n");

      // XXX
      return elle::Status::Ok;

      // if the scope is already being taking care of, ignore this
      // supervision.
      if (this->state != Scope::StateNone)
        return elle::Status::Ok;

      // if this scope is anonymous, i.e has been created, there is no
      // need to refresh it nor too disclose its modifications since nobody
      // can load it but the actor having created it.
      if (this->chemin == path::Chemin::Null)
        return elle::Status::Ok;

      // depending on the context's state.
      switch (this->context->state)
        {
        case Context::StateUnknown:
          {
            throw Exception(elle::sprintf("unexpected state '%u'",
                                                this->context->state));
          }
        case Context::StateJournaled:
          {
            // nothing to do.

            break;
          }
        case Context::StateLoaded:
        case Context::StateDiscarded:
          {
            // set the state.
            this->state = Scope::StateRefreshing;

            // perform the refreshing depending on the context's nature.
            switch (this->context->nature)
              {
              case NatureUnknown:
                {
                  // reset the state.
                  this->state = Scope::StateNone;

                  throw Exception("unknown context nature");
                }
              case NatureGroup:
                {
                  printf("[XXX] to handle\n");

                  break;
                }
              case NatureObject:
                {
                  // refresh the scope.
                  if (this->Refresh<gear::Object>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to refresh the scope");
                    }

                  break;
                }
              case NatureFile:
                {
                  // refresh the scope.
                  if (this->Refresh<gear::File>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to refresh the scope");
                    }

                  break;
                }
              case NatureDirectory:
                {
                  // refresh the scope.
                  if (this->Refresh<gear::Directory>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to refresh the scope");
                    }

                  break;
                }
              case NatureLink:
                {
                  // refresh the scope.
                  if (this->Refresh<gear::Link>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to refresh the scope");
                    }

                  break;
                }
              }

            // reset the state.
            this->state = Scope::StateNone;

            break;
          }
        case Context::StateCreated:
        case Context::StateModified:
        case Context::StateStored:
        case Context::StateDestroyed:
          {
            // set the state.
            this->state = Scope::StateDisclosing;

            // perform the disclosure depending on the context's nature.
            switch (this->context->nature)
              {
              case NatureUnknown:
                {
                  // reset the state.
                  this->state = Scope::StateNone;

                  throw Exception("unknown context nature");
                }
              case NatureGroup:
                {
                  printf("[XXX] to handle\n");

                  break;
                }
              case NatureObject:
                {
                  // disclose the scope.
                  if (this->Disclose<gear::Object>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to disclose the scope");
                    }

                  break;
                }
              case NatureFile:
                {
                  // disclose the scope.
                  if (this->Disclose<gear::File>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to disclose the scope");
                    }

                  break;
                }
              case NatureDirectory:
                {
                  // disclose the scope.
                  if (this->Disclose<gear::Directory>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to disclose the scope");
                    }

                  break;
                }
              case NatureLink:
                {
                  // disclose the scope.
                  if (this->Disclose<gear::Link>() == elle::Status::Error)
                    {
                      // reset the state.
                      this->state = Scope::StateNone;

                      throw Exception("unable to disclose the scope");
                    }

                  break;
                }
              }

            // reset the state.
            this->state = Scope::StateNone;

            break;
          }
        }

      return elle::Status::Ok;
    }

    void
    Scope::print(std::ostream& out) const
    {
      out << "<gear::Scope at " << this << " of " << this->chemin;
      if (this->actors.size())
        {
          out << " with actors: ";
          bool is_first = true;
          for (auto const& actor: this->actors)
            {
              if (is_first)
                is_first = false;
              else
                out << ", ";
              out << actor->identifier;
            }
          out << ")";
        }
        out << ">";
    }

//
// ---------- dumpable --------------------------------------------------------
//

    ///
    /// this method dumps a scope.
    ///
    elle::Status        Scope::Dump(const elle::Natural32       margin) const
    {
      elle::String      alignment(margin, ' ');
      Scope::A::Scoutor scoutor;

      std::cout << alignment << "[Scope] " << std::hex << this << std::endl;

      // dump the chemin.
      if (this->chemin.Dump(margin + 2) == elle::Status::Error)
        throw Exception("unable to dump the chemin");

      // dump the context, if present.
      if (this->context != nullptr)
        {
          if (this->context->Dump(margin + 2) == elle::Status::Error)
            throw Exception("unable to dump the context");
        }
      else
        {
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Context] " << "none" << std::endl;
        }

      // dump the chronicle, if present.
      if (this->chronicle != nullptr)
        {
          if (this->chronicle->Dump(margin + 2) == elle::Status::Error)
            throw Exception("unable to dump the chronicle");
        }
      else
        {
          std::cout << alignment << elle::io::Dumpable::Shift
                    << "[Chronicle] " << "none" << std::endl;
        }

      std::cout << alignment << elle::io::Dumpable::Shift
                << "[Actors]" << std::endl;

      // go through the actors.
      for (scoutor = this->actors.begin();
           scoutor != this->actors.end();
           scoutor++)
        {
          // dump the actor.
          if ((*scoutor)->Dump(margin + 4) == elle::Status::Error)
            throw Exception("unable to dump the actor");
        }

      return elle::Status::Ok;
    }

  }
}
