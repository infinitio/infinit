#ifndef ORACLES_HERMES_CLERK_HH
# define ORACLES_HERMES_CLERK_HH

# include <boost/algorithm/string.hpp>
# include <boost/filesystem.hpp>
# include <boost/lexical_cast.hpp>

# include <algorithm>
# include <fstream>
# include <vector>

# include <elle/Buffer.hh>

# include <infinit/oracles/hermes/Chunk.hh>

namespace oracles
{
  namespace hermes
  {
    typedef std::string TID;

    class Clerk
    {
    public:
      Clerk(boost::filesystem::path& base_path);

      static
      void
      check_directory(boost::filesystem::path& path);

    public:
      void
      ident(TID id);

      Size
      store(FileID id, Offset off, elle::Buffer& buff);

      elle::Buffer
      fetch(FileID id, Offset off, Size size);

    private:
      void
      _explore(boost::filesystem::path& path);

    private:
      std::vector<Chunk> _chunks;
      boost::filesystem::path _base_path;
      bool _identified;
    };
  }
}

#endif // !ORACLES_HERMES_CLERK_HH
