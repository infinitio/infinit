#include <elle/io/Directory.hh>
#include <elle/io/File.hh>
#include <elle/io/Link.hh>
#include <elle/io/Path.hh>
#include <elle/system/platform.hh>
#include <elle/system/system.hh>
#include <elle/printf.hh>

#include <sstream>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <elle/os/path.hh>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/types.h>
#include <dirent.h>

// XXX[to rework completely]
namespace // usefull classes
{

  ///
  /// This class abstracts low level calls to
  /// iterate through directories
  ///
  class _Directory : private boost::noncopyable
  {
  public:
    class iterator
    {
    private:
      ::DIR*            _dir;
      struct ::dirent*  _entry;
      elle::String      _current;

    public:
      iterator(::DIR* dir) :
        _dir(dir), _entry(nullptr), _current()
      {
        if (dir != nullptr)
        {
          this->_entry = ::readdir(this->_dir);
          if (this->_entry != nullptr)
            this->_current.assign(this->_entry->d_name);
        }
      }
      iterator(iterator const& other) :
        _dir(other._dir), _entry(other._entry), _current(other._current)
      {}

      iterator& operator ++()
      {
        if (this->_entry == nullptr)
          throw std::range_error("No more entry in this directory");
        this->_entry = ::readdir(this->_dir);
        if (this->_entry != nullptr)
          this->_current.assign(this->_entry->d_name);
        return *this;
      }

      bool operator ==(iterator const& other) const
      { return this->_entry == other._entry; }

      bool operator !=(iterator const& other) const
      { return this->_entry != other._entry; }

      elle::String const& operator*() const
      {
        if (this->_entry == nullptr)
          throw std::runtime_error("Access to an invalid directory iterator");
        return this->_current;
      }

      elle::String const* operator ->() const
      {
        if (this->_entry == nullptr)
          throw std::runtime_error("Access to an invalid directory iterator");
        return &this->_current;
      }

    private:
      iterator& operator=(iterator const&); // yes, I'm lazy
    };

  private:
    ::DIR* _dir;

  public:
    _Directory(elle::io::Path const& path) :
      _dir(::opendir(path.string().c_str()))
    {}
    ~_Directory()
    {
      if (_dir != nullptr)
        ::closedir(this->_dir);
    }
    operator bool() const { return this->_dir != nullptr; }
    iterator begin() { return iterator(this->_dir); }
    iterator end() { return iterator(nullptr); }
  };

} // !ns anonymous

namespace elle
{
  namespace io
  {

//
// ---------- static methods --------------------------------------------------
//

    ///
    /// this method creates a directory at the given path.
    ///
    Status              Directory::Create(const Path&           path)
    {
      try
        {
          os::path::make_path(path.string());
        }
      catch (std::exception const & e)
        {
          throw Exception(elle::sprintf("couldn't make path '%s': %s",
                                        path.string().c_str(), e.what()));
        }

      return Status::Ok;

      // XXX
#if 0
      // does the directory already exist.
      if (Directory::Exist(path) == true)
        throw Exception("the directory seems to already exist");

      // dig the directory which will hold the target directory.
      if (Directory::Dig(path) == Status::Error)
        throw Exception("unable to dig the chain of directories");

      // create the directory.
#if defined(INFINIT_LINUX) || defined(INFINIT_MACOSX)
      if (::mkdir(path.string.c_str(), 0700) != 0)
        throw Exception(::strerror(errno));
#elif defined(INFINIT_WINDOWS)
      if (::mkdir(path.string.c_str()) != 0)
        throw Exception("unable to create %s: %s",
               path.string.c_str(), ::strerror(errno));
#else
# error "unsupported platform"
#endif

      return Status::Ok;
#endif
    }

    ///
    /// this method removes a directory.
    ///
    Status              Directory::Remove(const Path&           path)
    {
      // does the directory exist.
      if (Directory::Exist(path) == false)
        throw Exception("the directory does not seem to exist");

      // remove the directory.
      ::rmdir(path.string().c_str());

      return Status::Ok;
    }

    ///
    /// this method returns true if the pointed to directory exists.
    ///
    Boolean             Directory::Exist(const Path&                    path)
    {
      struct ::stat             stat;

      // does the path points to something.
      if (::stat(path.string().c_str(), &stat) != 0)
        return false;

      // does the path points to a directory.
      if (!S_ISDIR(stat.st_mode))
        return false;

      return true;
    }

    ///
    /// this method takes a path to a directory and creates, if necessary,
    /// the intermediate directories leading to the target.
    ///
    Status              Directory::Dig(const Path&                      path)
    {
      String            directory(::dirname(
                                    const_cast<char*>(path.string().c_str())));
      try
        {
          os::path::make_path(directory);
        }
      catch (std::exception const & e)
        {
          throw Exception(elle::sprintf("couldn't make path '%s': %s",
                                        directory, e.what()));
        }

      return Status::Ok;

      // XXX
#if 0
      String            target(::strdup(path.string.c_str()));
      String            directory(::dirname(
                                    const_cast<char*>(target.c_str())));
      std::stringstream stream(directory);
      String            item;
      Path              chemin;

      // go through the components of the path.
      while (std::getline(stream, item, system::path::separator))
        {
          // update the intermediate chemin.
          if (chemin.string.empty() && item.empty())
            chemin.string = system::path::separator;
          else
            {
              chemin.string.append(item);
              chemin.string.append(1, system::path::separator);
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
#endif
    }


    ///
    /// this method recursively removes everything in the given directory.
    ///
    Status              Directory::Clear(const Path&            path)
    {
      // is the path pointing to a valid directory.
      if (Directory::Exist(path) == false)
        throw Exception("the path does not reference a directory");

      _Directory directory(path);

      // open the directory.
      if (!directory)
        throw Exception("unable to open the directory");

      auto it = directory.begin();
      auto end = directory.end();
      for (; it != end; ++it)
        {
          Path          target;
          struct ::stat stat;

          // ignore the . and ..
          if (*it == "." || *it == "..")
            continue;

          // create the target path.
          String path_str(path.string() +
                          system::path::separator +
                          *it);
          if (target.Create(path_str) == Status::Error)
            throw Exception("unable to create the target path");

          // stat the entry as entry->d_type is not standard
#if defined(INFINIT_LINUX) || defined(INFINIT_MACOSX)
          if (::lstat(target.string().c_str(), &stat) == -1)
#elif defined(INFINIT_WINDOWS)
          if (::stat(target.string().c_str(), &stat) == -1)
#else
# error "unsupported platform"
#endif
            // the stat may fail but it's ok to continue as it's not fatal
            // and the entry may have been destroyed/moved between the readdir
            // and now.
            continue;

          // perform an action depending on the nature of the target.
          if (S_ISDIR(stat.st_mode))
            {
              // empty it as well.
              if (Directory::Clear(target) == Status::Error)
                throw Exception("unable to empty a subdirectory");

              // remove the directory.
              if (Directory::Remove(target) == Status::Error)
                throw Exception("unable to remove the subdirectory");
            }
          else if (S_ISREG(stat.st_mode))
            {
              // remove the file.
              if (File::Erase(target) == Status::Error)
                throw Exception("unable to remove the file");
            }
#if defined(INFINIT_LINUX) || defined(INFINIT_MACOSX)
          else if (S_ISLNK(stat.st_mode))
            {
              // remove the link.
              if (Link::Erase(target) == Status::Error)
                throw Exception("unable to remove the link");
            }
#endif
          else
            throw Exception("unhandled file system object type");
        }

      return Status::Ok;
    }

    ///
    /// this method returns a list of directory entries.
    ///
    Status              Directory::List(const Path&             path,
                                        Set&                    set)
    {
      _Directory directory(path);

      // open the directory.
      if(!directory)
        throw Exception("unable to open the directory");

      // go through the entries.
      auto it = directory.begin(),
           end = directory.end();
      for (; it != end; ++it)
        {
          // ignore the '.' and '..' entries.
          if (*it != "." && *it != "..")
            set.push_back(*it);
        }

      return Status::Ok;
    }

  }
}
