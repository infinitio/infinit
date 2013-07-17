#ifndef ETOILE_AUTOMATON_LINK_HH
# define ETOILE_AUTOMATON_LINK_HH

# include <elle/types.hh>
# include <etoile/path/fwd.hh>
# include <etoile/gear/fwd.hh>

namespace etoile
{
  namespace automaton
  {

    ///
    /// this class provides functionalities for managing directories.
    ///
    class Link
    {
    public:
      //
      // static methods
      //
      static elle::Status       Create(gear::Link&);
      static elle::Status       Load(gear::Link&);

      static elle::Status
      Bind(gear::Link&,
           std::string const& path);
      static elle::Status
      Resolve(gear::Link& context,
              std::string& path);

      static elle::Status       Discard(gear::Link&);
      static elle::Status       Store(gear::Link&);
      static elle::Status       Destroy(gear::Link&);
    };

  }
}

#endif
