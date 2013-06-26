#include <etoile/wall/Path.hh>
#include <etoile/path/Path.hh>
#include <etoile/path/Chemin.hh>
#include <etoile/Exception.hh>

#include <elle/log.hh>

#include <Infinit.hh>

ELLE_LOG_COMPONENT("infinit.etoile.wall.Path");

namespace etoile
{
  namespace wall
  {
    path::Chemin
    Path::resolve(std::string const& way)
    {
      ELLE_TRACE_FUNCTION(way);

      path::Route route(way);
      path::Venue venue;

      // Resolve the route.
      if (path::Path::Resolve(route, venue) == elle::Status::Error)
        throw Exception("unable to resolve the route");

      return path::Chemin(route, venue);
    }

    // XXX[to move somewhere else]
    NoSuchFileOrDirectory::NoSuchFileOrDirectory(reactor::Scheduler& sched,
                                                 std::string const& path):
      reactor::Exception(elle::sprintf("no such file or directory: %s", path)),
      _path(path)
    {}

    // XXX[to move somewhere else]
    NoSuchFileOrDirectory::~NoSuchFileOrDirectory() throw ()
    {}
  }
}
