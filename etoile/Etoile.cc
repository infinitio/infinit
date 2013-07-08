#include <etoile/Etoile.hh>
#include <etoile/Exception.hh>
#include <etoile/gear/Actor.hh>
#include <etoile/gear/Scope.hh>
#include <etoile/path/Path.hh>
#include <etoile/shrub/Shrub.hh>

#include <elle/log.hh>

ELLE_LOG_COMPONENT("infinit.etoile.Etoile");

namespace etoile
{
  /*-------------.
  | Construction |
  `-------------*/

  namespace time = boost::posix_time;
  Etoile::Etoile(infinit::cryptography::KeyPair const& user_keypair,
                 hole::Hole* hole,
                 nucleus::proton::Address const& root_address):
    _user_keypair(user_keypair),
    _user_subject(this->_user_keypair.K()),
    _actors(),
    _shrub(),
    _depot(hole, root_address)
  {
  }

  Etoile::~Etoile()
  {
    auto it = begin(this->_actors);
    for (; it != end(this->_actors); it = begin(this->_actors))
    {
      delete it->second;
    }

    this->_onymous_scopes.clear();
    this->_anonymous_scopes.clear();
  }

  /*-------.
  | Scopes |
  `-------*/

  std::shared_ptr<gear::Scope>
  Etoile::scope_acquire(const path::Chemin& chemin)
  {
    ELLE_TRACE_METHOD(chemin);

    if (this->_scope_exist(chemin) == false)
    {
      auto scope = std::shared_ptr<gear::Scope>(new gear::Scope(chemin));

      this->_scope_inclose(scope);

      return (scope);
    }
    else
      return (this->_scope_retrieve(chemin));

    elle::unreachable();
  }

  std::shared_ptr<gear::Scope>
  Etoile::scope_supply()
  {
    ELLE_TRACE_METHOD("");

    auto scope = std::shared_ptr<gear::Scope>(new gear::Scope);

    this->_scope_inclose(scope);

    return (scope);
  }

  void
  Etoile::scope_annihilate(std::shared_ptr<gear::Scope> const& scope)
  {
    ELLE_TRACE_METHOD(scope);

    if (scope->actors.empty() == true)
    {
      this->scope_relinquish(scope);

      ELLE_ASSERT_EQ(scope.unique(), true);
    }
  }

  void
  Etoile::scope_relinquish(std::shared_ptr<gear::Scope> const& scope)
  {
    ELLE_TRACE_METHOD(scope);

    if (scope->chemin.empty())
      this->_scope_remove(scope);
    else
      this->_scope_remove(scope->chemin);
  }

  void
  Etoile::scope_relinquish(gear::Scope const* scope)
  {
    ELLE_TRACE_METHOD(scope);

    if (scope->chemin.empty())
    {
      // XXX iterate manually since we are forced to handle
      //     a raw pointer.
      for (auto anonymous_scope: this->_anonymous_scopes)
      {
        if (anonymous_scope.get() == scope)
        {
          this->_scope_remove(anonymous_scope);
          return;
        }
      }

      throw Exception(
        elle::sprintf("unknown onymous scope: '%s'", *scope));
    }
    else
      this->_scope_remove(scope->chemin);
  }

  /// XXX note that the mechanism below is not very efficient.
  ///     instead a tree-based data structure should be used
  ///     in order to update the container in an efficient
  ///     manner.
  void
  Etoile::scope_update(const path::Chemin& from,
                       const path::Chemin& to)
  {
    ELLE_DEBUG_METHOD(from, to);

  retry:
    auto iterator = this->_onymous_scopes.begin();
    auto end = this->_onymous_scopes.end();

    for (; iterator != end; ++iterator)
    {
      std::shared_ptr<gear::Scope> scope = iterator->second;

      if (scope->chemin.derives(from) == false)
        continue;

      // the scope's chemin seems to derive the base
      // i.e _from_.
      //
      // it must therefore be updated so as to be
      // consistent.

      scope->chemin = to;

      // note that the current scope is registered in
      // the container with its old chemin as the key.
      //
      // therefore, the container's key for this scope
      // must also be updated.
      //
      // the following thus removes the scope and
      // re-inserts it.

      this->_scope_remove(from);
      this->_scope_add(scope->chemin, std::move(scope));

      // at this point, we cannot go further with the
      // iterator as consistency cannot be guaranteed
      // anymore.
      //
      // therefore, go through the whole process again.

      goto retry;
    }
  }

  void
  Etoile::scope_show() const
  {
    std::cout << "onymous" << std::endl;
    for (auto pair: this->_onymous_scopes)
      pair.second->Dump(2);

    std::cout << "anonymous" << std::endl;
    for (auto scope: this->_anonymous_scopes)
      scope->Dump(2);
  }

  elle::Boolean
  Etoile::_scope_exist(const path::Chemin& chemin) const
  {
    ELLE_DEBUG_METHOD(chemin);

    for (auto pair: this->_onymous_scopes)
      if (pair.second->chemin == chemin)
        return true;

    return false;
  }

  void
  Etoile::_scope_add(const path::Chemin& chemin,
                     std::shared_ptr<gear::Scope> scope)
  {
    ELLE_DEBUG_METHOD(chemin, scope);

    auto result = this->_onymous_scopes.insert(
      std::pair< const path::Chemin, std::shared_ptr<gear::Scope> >(
        chemin, scope));

    if (result.second == false)
      throw Exception("unable to insert the scope in the container");
  }

  std::shared_ptr<gear::Scope>
  Etoile::_scope_retrieve(const path::Chemin& chemin) const
  {
    ELLE_DEBUG_METHOD(chemin);

    auto iterator = this->_onymous_scopes.find(chemin);

    if (iterator == this->_onymous_scopes.end())
      throw Exception("unable to locate the scope associated with "
                      "the given chemin");

    return (iterator->second);
  }

  void
  Etoile::_scope_remove(const path::Chemin& chemin)
  {
    ELLE_DEBUG_METHOD(chemin);

    auto iterator = this->_onymous_scopes.find(chemin);

    if (iterator == this->_onymous_scopes.end())
      throw Exception("unable to locate the scope associated with "
                      "the given chemin");

    this->_onymous_scopes.erase(iterator);
  }

  void
  Etoile::_scope_add(std::shared_ptr<gear::Scope> scope)
  {
    ELLE_DEBUG_METHOD(scope);

    this->_anonymous_scopes.push_back(scope);
  }

  void
  Etoile::_scope_remove(std::shared_ptr<gear::Scope> const& scope)
  {
    ELLE_DEBUG_METHOD(scope);

    this->_anonymous_scopes.remove(scope);
  }

  void
  Etoile::_scope_inclose(std::shared_ptr<gear::Scope> scope)
  {
    ELLE_DEBUG_METHOD(scope);

    if (scope->chemin.empty())
      this->_scope_add(scope);
    else
      this->_scope_add(scope->chemin, scope);
  }

  /*-------.
  | Actors |
  `-------*/

  gear::Actor*
  Etoile::actor_get(gear::Identifier const& id) const
  {
    auto it = this->_actors.find(id);
    if (it == this->_actors.end())
      throw elle::Exception(elle::sprintf("unkown actor %s", id));
    return it->second;
  }

  void
  Etoile::actor_add(gear::Actor& actor)
  {
    auto id = actor.identifier;
    if (this->_actors.find(id) != this->_actors.end())
      throw elle::Exception(elle::sprintf("duplicate actor %s", id));
    this->_actors[id] = &actor;
  }

  void
  Etoile::actor_remove(gear::Actor const& actor)
  {
    auto id = actor.identifier;
    auto it = this->_actors.find(id);
    if (it == this->_actors.end())
      throw elle::Exception(elle::sprintf("unkown actor %s", id));
    this->_actors.erase(it);
  }

  /*------.
  | Depot |
  `------*/

  nucleus::proton::Network const&
  Etoile::network() const
  {
    return this->_depot.network();
  }
}
