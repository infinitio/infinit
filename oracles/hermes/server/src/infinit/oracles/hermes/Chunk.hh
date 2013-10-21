#ifndef ORACLES_HERMES_CHUNK_HH
# define ORACLES_HERMES_CHUNK_HH

# include <boost/lexical_cast.hpp>
# include <boost/algorithm/string.hpp>

# include <fstream>
# include <elle/Buffer.hh>
# include <frete/Frete.hh>

namespace oracles
{
  namespace hermes
  {
    typedef frete::Frete::FileID FileID;
    typedef frete::Frete::Offset Offset;
    typedef frete::Frete::Size Size;
    class Clerk;

    class Chunk
    {
    public:
      Chunk(boost::filesystem::path& path, FileID id, Offset off, Size s);
      Chunk(boost::filesystem::path const& path);

    public:
      bool
      follows(Chunk const& other) const;

      bool
      overlaps(Chunk const& other) const;

      bool
      leads(Chunk const& other) const;

      bool
      belongs_to(Chunk const& other) const;

    public:
      void
      append(elle::Buffer& buff);

      void
      save(elle::Buffer& buff);

      void
      merge(Chunk const& other, elle::Buffer& buff);

      void
      prepend(Chunk& other, elle::Buffer& buff);

      elle::Buffer
      extract(Chunk const& piece) const;

      void
      remove();

    private:
      std::string _name();

    private:
      boost::filesystem::path _path;

      FileID _id;
      Offset _off;
      Size _size;
    };
  }
}

#endif // !ORACLES_HERMES_CHUNK_HH
