#include <oracle/disciples/hermes/src/hermes/Clerk.hh>
// TODO: Correct include path.
// TODO: learn to pass exact exceptions through network

namespace oracle
{
  namespace hermes
  {
    Clerk::Clerk(std::string base_path):
      _base_path(base_path)
    {
      if (not boost::filesystem::exists(base_path))
        boost::filesystem::create_directory(base_path);
      else if (not boost::filesystem::is_directory(base_path))
        throw elle::Exception("Clerk base path is not valid");

      // Build chunks list from files present in base_path at startup.
      boost::filesystem::directory_iterator end_iter;
      for (boost::filesystem::directory_iterator dir_iter(_base_path);
           dir_iter != end_iter; ++dir_iter)
      {
        if (boost::filesystem::is_regular_file(dir_iter->path()))
          try
          {
            _chunks.push_back(dir_iter->path());
          }
          catch (boost::bad_lexical_cast const&)
          {}
      }
    }

    Size
    Clerk::store(FileID id, Offset off, elle::Buffer& buff)
    {
      ChunkMeta current(id, off);

      if (find(_chunks.begin(), _chunks.end(), current) == _chunks.end())
      {
        _chunks.push_back(current);
        return _save(current, buff);
      }

      throw elle::Exception("Chunk already present");
    }

    elle::Buffer
    Clerk::serve(FileID id, Offset off)
    {
      ChunkMeta current(id, off);

      if (find(_chunks.begin(), _chunks.end(), current) == _chunks.end())
        throw elle::Exception("Chunk not found");

      return _retrieve(current);
    }

    Size
    Clerk::_save(ChunkMeta const& chunk, elle::Buffer& buff) const
    {
      std::ofstream out(chunk.path(_base_path).c_str());

      if (not out.is_open())
        throw elle::Exception("Error while trying to save chunk");

      out.write(reinterpret_cast<char*>(buff.mutable_contents()), buff.size());
      out.close();

      if (out.fail())
        throw elle::Exception("Error while trying to save chunk");

      return buff.size();
    }

    elle::Buffer
    Clerk::_retrieve(ChunkMeta const& chunk) const
    {
      Size size(boost::filesystem::file_size(chunk.path(_base_path)));

      elle::Buffer buff(size);
      std::ifstream in(chunk.path(_base_path).c_str());

      if (not in.is_open())
        throw elle::Exception("Could not retrieve chunk");

      in.readsome(reinterpret_cast<char*>(buff.mutable_contents()), size);

      in.close();
      return buff;
    }

    ChunkMeta::ChunkMeta(FileID id, Offset off):
      _id(id),
      _off(off)
    {}

    ChunkMeta::ChunkMeta(boost::filesystem::path const& path)
    {
      std::string const name(path.filename().replace_extension().string());
      std::vector<std::string> values;
      boost::split(values, name, boost::is_any_of("_"));

      _id = boost::lexical_cast<FileID>(values[0]);
      _off = boost::lexical_cast<Offset>(values[1]);
    }

    boost::filesystem::path
    ChunkMeta::path(boost::filesystem::path root) const
    {
      return root / boost::filesystem::path(string() + ".blk");
    }

    bool
    ChunkMeta::operator ==(ChunkMeta const& other)
    {
      return other._id == _id and other._off == _off;
    }

    const std::string
    ChunkMeta::string() const
    {
      std::string out(std::to_string(_id) + "_" + std::to_string(_off));
      return out;
    }
  }
}
