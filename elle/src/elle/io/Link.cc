#include <elle/io/Link.hh>
#include <elle/io/File.hh>
#include <elle/io/Directory.hh>
#include <elle/io/Path.hh>
#include <elle/os/path.hh>

#include <elle/system/system.hh>
#include <elle/system/platform.hh>

#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#if defined(INFINIT_WINDOWS)
# include <windows.h>
#endif

namespace elle
{
  namespace io
  {

//
// ---------- static methods --------------------------------------------------
//

    ///
    /// this method creates a link.
    ///
    Status              Link::Create(const Path&                link,
                                     const Path&                target)
    {
      // does the link exist.
      if (Link::Exist(link) == true)
        throw Exception("the link seems to already exist");

      // does the target exist.
      if ((File::Exist(target) == false) &&
          (Directory::Exist(target) == false))
        throw Exception("the target does not seem to exist");

      // create the link.
#if defined(INFINIT_LINUX) || defined(INFINIT_MACOSX)
      if (::symlink(target.string().c_str(), link.string().c_str()))
        throw Exception("symlink failed: %s -> %s: %s", link.string().c_str(),
               target.string().c_str(), ::strerror(errno));
#elif defined(INFINIT_WINDOWS)
      elle::os::path::make_symlink(link.string(), target.string());
      if (elle::os::path::check_symlink(target.string()))
        throw Exception("Symlink failed: '%s' -> '%s'.", link.string().c_str(),
               target.string().c_str());
#else
# error "unsupported platform"
#endif

      return Status::Ok;
    }

    ///
    /// this method erases the given link path.
    ///
    Status              Link::Erase(const Path&                 path)
    {
      // does the link exist.
      if (Link::Exist(path) == false)
        throw Exception("the link does not seem to exist");

      // unlink the link.
      ::unlink(path.string().c_str());

      return Status::Ok;
    }

    ///
    /// this method returns true if the pointed to link exists.
    ///
    Boolean             Link::Exist(const Path&                 path)
    {
      struct ::stat             stat;

#if defined(INFINIT_LINUX) || defined(INFINIT_MACOSX)
      // does the path points to something.
      if (::lstat(path.string().c_str(), &stat) != 0)
        return false;

      // does the path points to a regular file.
      if (!S_ISLNK(stat.st_mode))
        return false;
#elif defined(INFINIT_WINDOWS)
      // does the path points to something.
      if (::stat(path.string().c_str(), &stat) != 0)
        return false;
#else
# error "unsupported platform"
#endif

      return true;
    }

    ///
    /// this method takes a path to a link and creates, if necessary,
    /// the intermediate directory leading to the file.
    ///
    Status              Link::Dig(const Path&                   path)
    {
      String            target(path.string());
      char *            tmp_str = ::strdup(path.string().c_str());
      String            directory(::dirname(tmp_str));
      std::stringstream stream(directory);
      String            item;
      Path              chemin;

      // free the temporary string used for directory
      free(tmp_str);

      // go through the components of the path.
      while (std::getline(stream, item, system::path::separator))
        {
          // update the intermediate chemin.
          if (chemin.string().empty() && item.empty())
            chemin.string() = system::path::separator;
          else
            {
              chemin.string().append(item);
              chemin.string().append(1, system::path::separator);
            }

          // retrieve information on the path. should this operation fail
          // would mean that the target directory does not exist.
          if (Directory::Exist(chemin) == false)
            {
              // create the intermediate directory.
              if (Directory::Create(chemin) == Status::Error)
                throw Exception("unable to create the intermediate directory");
            }
        }

      return Status::Ok;
    }

  }
}
