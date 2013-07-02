#ifndef ETOILE_GEAR_SCOPE_HH
# define ETOILE_GEAR_SCOPE_HH

# include <list>

# include <elle/types.hh>
# include <elle/Printable.hh>

# include <etoile/gear/fwd.hh>
# include <etoile/gear/Actor.hh>
# include <etoile/gear/Operation.hh>
# include <etoile/path/Chemin.hh>
# include <etoile/Etoile.hh>

# include <reactor/rw-mutex.hh>

# include <boost/noncopyable.hpp>

namespace etoile
{
  namespace gear
  {

    ///
    /// this class represents a scope on which to operate in order to
    /// manipulate a file system object such as a file, directory or else.
    ///
    /// note that this class also maintains a static data structure holding
    /// the living scope according to their chemin so that whenever an
    /// actor tries to load a already loaded scope, the actor is simply
    /// attached to the scope.
    ///
    /// finally, note that anonymous scope are also kept. indeed, a file
    /// created by an application for instance does not have a chemin
    /// since the file has not be attached to the file system hierarchy
    /// yet. therefore, such scope are maintained in an anonymous data
    /// structure for as long as necessary.
    ///
    class Scope:
      public elle::Printable,
      private boost::noncopyable
    {
    public:
      //
      // enumerations
      //
      enum State
        {
          StateNone,
          StateRefreshing,
          StateDisclosing
        };

      //
      // types
      //
      typedef std::list<Actor*>                       Container;
      typedef typename Container::iterator            Iterator;
      typedef typename Container::const_iterator      Scoutor;

      //
      // constructors & destructors
      //
      Scope();
      Scope(const path::Chemin&);
      ~Scope();
    private:
      Scope(Scope&& other);

      //
      // methods
      //
    public:
      elle::Boolean     Locate(Actor*,
                               Iterator* = nullptr);

      elle::Status      Attach(Actor*);
      elle::Status      Detach(Actor*);

      template <typename T>
      elle::Status      Use(Etoile& etoile,
                            T*&);

      elle::Status      Operate(const Operation);

      elle::Status      Shutdown();

      template <typename T>
      elle::Status      _shutdown();

      template <typename T>
      elle::Status      Refresh();
      template <typename T>
      elle::Status      Disclose();

    public:
      void
      print(std::ostream& out) const;

      //
      // callbacks
      //
      elle::Status      Supervisor();

      //
      // interfaces
      //

      // dumpable
      elle::Status      Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      State             state;

      path::Chemin      chemin;

      Context*          context;
      Chronicle*        chronicle;

      Container      actors;

      reactor::RWMutex mutex;
    };

  }
}

#include <etoile/gear/Scope.hxx>

#endif
