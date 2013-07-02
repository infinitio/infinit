#ifndef ETOILE_GEAR_GUARD_HH
# define ETOILE_GEAR_GUARD_HH

# include <elle/types.hh>

# include <etoile/gear/fwd.hh>

# include <memory>

namespace etoile
{
  namespace gear
  {

    ///
    /// this class enables one to easily manipulate scopes and actor so
    /// as to make sure they get deleted if an error occurs.
    ///
    class Guard
    {
      //
      // constructors & destructors
      //
    public:
      Guard(std::shared_ptr<Scope> const&,
            Actor* = nullptr);
      Guard(Actor*);
      ~Guard();

      //
      // methods
      //
    public:
      elle::Status      Release();

      //
      // getters & setters
      //
      Actor*            actor();
      elle::Void        actor(Actor*);

      //
      // attributes
      //
    private:
      std::shared_ptr<Scope> _scope;
      Actor*            _actor;
      elle::Boolean     _track;
    };

  }
}

#endif
