#ifndef ORACLE_DISCIPLES_HERMES_CHUNK_HH
# define ORACLE_DISCIPLES_HERMES_CHUNK_HH

# include <fstream>
# include <elle/Buffer.hh>
# include <frete/Frete.hh>

namespace oracle
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
      Chunk(std::string const& config_line);

    public:
      bool follows(Chunk const& other) const;
      bool overlaps(Chunk const& other) const;
      bool leads(Chunk const& other) const;

    public:
      void append(elle::Buffer& buff);
      void save(elle::Buffer& buff);
      void merge(Chunk const& other, elle::Buffer& buff);
      void prepend(Chunk& other, elle::Buffer& buff);
      void remove();

      //TODO: change to private
    public:
      std::string _name();

    private:
      boost::filesystem::path _path;

      FileID _id;
      Offset _off;
      Size _size;
    };
  }
}

#endif // !ORACLE_DISCIPLES_HERMES_CHUNK_HH
