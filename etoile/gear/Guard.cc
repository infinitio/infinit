#include <etoile/gear/Guard.hh>
#include <etoile/gear/Actor.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/Etoile.hh>

#include <elle/log.hh>

namespace etoile
{
  namespace gear
  {

//
// ---------- constructors & destructors --------------------------------------
//

    ///
    /// default constructor.
    ///
    Guard::Guard(std::shared_ptr<Scope> const& scope,
                 Actor*                                             actor):
      _scope(scope),
      _actor(actor),
      _track(true)
    {
    }

    ///
    /// another constructor.
    ///
    Guard::Guard(Actor* actor):
      _scope(nullptr),
      _actor(actor),
      _track(true)
    {
    }

    ///
    /// destructor.
    ///
    Guard::~Guard()
    {
      if (this->_track == true)
        {
          if (this->_actor != nullptr)
            delete this->_actor;

          if (this->_scope != nullptr && this->_scope->context != nullptr)
          {
            Etoile& etoile = this->_scope->context->etoile();

            etoile.scope_annihilate(this->_scope);
          }
        }
    }

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method releases the scope guarding.
    ///
    elle::Status            Guard::Release()
    {
      this->_track = false;

      return (elle::Status::Ok);
    }

//
// ---------- getters & setters -----------------------------------------------
//

    Actor*                  Guard::actor()
    {
      return (this->_actor);
    }

    elle::Void              Guard::actor(Actor*                     actor)
    {
      //
      // delete the previous actor, if different.
      //
      if (this->_actor != actor)
        delete this->_actor;

      this->_actor = actor;
    }

  }
}
