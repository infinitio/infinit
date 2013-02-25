#include <elle/system/platform.hh>
#include <elle/system/system.hh>

#include <elle/io/File.hh>
#include <elle/io/Directory.hh>
#include <elle/io/Path.hh>

#include <elle/Buffer.hh>

#include <vector>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
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

#if defined(INFINIT_LINUX) || defined(INFINIT_MACOSX)
    ///
    /// this method reads the given file's content.
    ///
    Status              File::Read(const Path&                  path,
                                   Buffer& data)
    {
      struct ::stat     status;
      int               fd;
      Natural32         roffset = 0;

      // does the file exist.
      if (File::Exist(path) == false)
        throw Exception(elle::sprintf
                        ("the file '%s' does not seem to exist", path));

      // retrieve information.
      if (::stat(path.string().c_str(), &status) == -1)
        throw Exception(::strerror(errno));

      // prepare the data.
      data.size(status.st_size);

      // open the file.
      if ((fd = ::open(path.string().c_str(), O_RDONLY)) == -1)
        throw Exception(elle::sprintf("failed to open %s: %s",
                                      path.string().c_str(),
                                      ::strerror(errno)));

      // read the file's content.
      while (roffset < data.size())
        {
          int rbytes = ::read(fd,
                              data.mutable_contents() + roffset,
                              data.size() - roffset);

          if (rbytes == 0)
            break;

          if (rbytes < 0)
            {
              if (errno == EAGAIN ||
                  errno == EINTR)
                continue;

              ::close(fd);

              throw Exception(elle::sprintf("read error: %s",
                                            ::strerror(errno)));
            }

          roffset += rbytes;
        }

      data.size(roffset);

      // close the file.
      ::close(fd);

      return Status::Ok;
    }

    ///
    /// this method writes the given data into the given file.
    ///
    Status              File::Write(const Path&                 path,
                                    Buffer const& data)
    {
      int               fd;
      Natural32         woffset = 0;

      // dig the directory which will hold the target file.
      if (File::Dig(path) == Status::Error)
        throw Exception("unable to dig the chain of directories");

      // open the file.
      if ((fd = ::open(path.string().c_str(),
                       O_CREAT | O_TRUNC | O_WRONLY,
                       0600)) == -1)
        throw Exception(::strerror(errno));

      // write the text to the file.
      while (woffset < data.size())
        {
          int           wbytes;

          wbytes = ::write(fd,
                           data.contents() + woffset,
                           data.size() - woffset);

          if (wbytes < 0)
            {
              if (errno == EAGAIN ||
                  errno == EINTR)
                continue;

              ::close(fd);
              throw Exception(::strerror(errno));
            }

          if (wbytes == 0)
            break;

          woffset += wbytes;
        }

      // close the file.
      ::close(fd);

      return Status::Ok;
    }
#elif defined(INFINIT_WINDOWS)
    ///
    /// this method reads the given file's content.
    ///
    Status              File::Read(const Path&                  path,
                                   standalone::Region& data)
    {
      struct ::stat     status;
      HANDLE            fd;
      DWORD             roffset = 0;

      // does the file exist.
      if (File::Exist(path) == false)
         throw Exception("the file '%s' does not seem to exist", path);

      // retrieve information.
      if (::stat(path.string().c_str(), &status) == -1)
        throw Exception(::strerror(errno));

      // prepare the data.
      if (data.Prepare(static_cast<Natural32>(status.st_size)) == Status::Error)
        throw Exception("unable to prepare the region");

      // open the file.
      fd = ::CreateFile(path.string().c_str(), GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                        nullptr);

      if (fd == INVALID_HANDLE_VALUE)
        throw Exception("failed to open %s", path.string().c_str());

      // read the file's content.
      while (roffset < data.capacity)
        {
          DWORD rbytes;

          BOOL succeed = ::ReadFile(fd,
                                    data.contents + roffset,
                                    data.capacity - roffset,
                                    &rbytes,
                                    nullptr);

          if (!succeed)
            {
              ::CloseHandle(fd);
              throw Exception("read error");
            }

          if (rbytes == 0)
            break;

          roffset += rbytes;
        }

      data.size = roffset;

      // close the file.
      ::CloseHandle(fd);

      return Status::Ok;
    }

    ///
    /// this method writes the given data into the given file.
    ///
    Status              File::Write(const Path&                 path,
                                    const standalone::Region& data)
    {
      HANDLE            fd;
      DWORD             woffset = 0;

      // dig the directory which will hold the target file.
      if (File::Dig(path) == Status::Error)
        throw Exception("unable to dig the chain of directories");

      // open the file.
      fd = ::CreateFile(path.string().c_str(), GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                        nullptr);

      if (fd == INVALID_HANDLE_VALUE)
        throw Exception("failed to open %s", path.string().c_str());

      // write the text to the file.
      while (woffset < data.size)
        {
          DWORD         wbytes;
          BOOL          succeed;

          succeed = ::WriteFile(fd, data.contents + woffset,
                                data.size - woffset, &wbytes, nullptr);

          if (!succeed)
            {
              ::CloseHandle(fd);

              throw Exception("write error");
            }

          if (wbytes == 0)
            break;

          woffset += wbytes;
        }

      // close the file.
      ::CloseHandle(fd);

      return Status::Ok;
    }
#else
# error "unsupported platform"
#endif

    ///
    /// this method erases the given file path.
    ///
    Status              File::Erase(const Path&                 path)
    {
      // does the file exist.
      if (File::Exist(path) == false)
        throw Exception(elle::sprintf("the file '%s' does not seem to exist",
                                      path));

      // unlink the file.
      ::unlink(path.string().c_str());

      return Status::Ok;
    }

    ///
    /// this method returns true if the pointed to file exists.
    ///
    Boolean              File::Exist(const Path&                 path)
    {
      struct ::stat             stat;

      // does the path points to something.
      if (::stat(path.string().c_str(), &stat) != 0)
        return false;

      // does the path points to a regular file.
      if (!S_ISREG(stat.st_mode))
        return false;

      return true;
    }

    ///
    /// this method takes a path to a file and creates, if necessary,
    /// the intermediate directory leading to the file.
    ///
    Status              File::Dig(const Path&                   path)
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
