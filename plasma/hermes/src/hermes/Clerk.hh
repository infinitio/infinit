#ifndef PLASMA_HERMES_CLERK_HH
# define PLASMA_HERMES_CLERK_HH

# include <boost/algorithm/string.hpp>
# include <boost/filesystem.hpp>
# include <boost/lexical_cast.hpp>

# include <algorithm>
# include <fstream>
# include <vector>

# include <elle/Buffer.hh>
# include <frete/Frete.hh>

namespace plasma
{
  namespace hermes
  {
    typedef frete::Frete::FileID FileID;
    typedef frete::Frete::Offset Offset;
    typedef frete::Frete::Size Size;

    class ChunkMeta
    {
    public:
      ChunkMeta(FileID id, Offset off);
      ChunkMeta(boost::filesystem::path const& path);

    public:
      boost::filesystem::path path(boost::filesystem::path root) const;
      bool operator ==(ChunkMeta const& other);

    private:
      const std::string string() const;

    private:
      FileID _id;
      Offset _off;
    };

    class Clerk
    {
    public:
      Clerk(std::string base_path);

      Size store(ChunkMeta const& current, elle::Buffer& buff);
      elle::Buffer serve(ChunkMeta const& current);

    private:
      Size _save(ChunkMeta const& chunk, elle::Buffer& buff) const;
      elle::Buffer _retrieve(ChunkMeta const& chunk) const;

    private:
      std::vector<ChunkMeta> _chunks;
      boost::filesystem::path _base_path;
    };
  }
}

#endif // !PLASMA_HERMES_CLERK_HH
