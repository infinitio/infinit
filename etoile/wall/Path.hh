#ifndef ETOILE_WALL_PATH_HH
# define ETOILE_WALL_PATH_HH

# include <elle/types.hh>

# include <reactor/exception.hh>

# include <etoile/Etoile.hh>
# include <etoile/path/fwd.hh>

namespace etoile
{
  namespace wall
  {
    /// Path resolving into locations.
    class Path
    {
    public:
      /// Resolve a string-based path.
      ///
      /// The path may contain version indicators for Etoile to use a specific
      /// version of the named directory, file etc. contained in the path.
      static
      path::Chemin
      resolve(etoile::Etoile& etoile, std::string const& path);
    };

    // XXX[to move somewhere else]
    class NoSuchFileOrDirectory: public reactor::Exception
    {
    public:
      NoSuchFileOrDirectory(reactor::Scheduler& sched, std::string const& path);
      ~NoSuchFileOrDirectory() throw ();
    private:
      std::string _path;
    };
  }
}

#endif
