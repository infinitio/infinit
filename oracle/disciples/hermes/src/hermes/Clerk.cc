#include <hermes/Clerk.hh>

namespace oracle
{
  namespace hermes
  {
    Clerk::Clerk(boost::filesystem::path& base_path):
      _base_path(base_path),
      _identified(false)
    {}

    void
    Clerk::check_directory(boost::filesystem::path& path)
    {
      if (not boost::filesystem::exists(path))
        boost::filesystem::create_directory(path);
      else if (not boost::filesystem::is_directory(path))
        throw elle::Exception("Directory path is not valid");
    }

    void
    Clerk::_explore(boost::filesystem::path& path)
    {
      check_directory(_base_path /= path);

      boost::filesystem::directory_iterator end;
      for (boost::filesystem::directory_iterator it(_base_path);
           it != end; it++)
        if (boost::filesystem::is_regular_file(it->status()))
          _chunks.push_back(it->path());
        else
          throw elle::Exception("Invalid file in transaction folder");
    }

    void
    Clerk::ident(TID id)
    {
      // TODO: Check stuff with Transaction ID?
      boost::filesystem::path relative_path(id);
      _explore(relative_path);

      _identified = true;
    }

    // TODO: Opti, fileID offset in vector table?
    Size
    Clerk::store(FileID id, Offset off, elle::Buffer& buff)
    {
      if (not _identified)
        throw elle::Exception("Trying to store something without identifying");

      Chunk chunk(_base_path, id, off, buff.size());

      std::vector<Chunk>::iterator result;
      auto cmpf = std::bind(&Chunk::follows, &chunk, std::placeholders::_1);
      auto cmpo = std::bind(&Chunk::overlaps, &chunk, std::placeholders::_1);
      auto cmpl = std::bind(&Chunk::leads, &chunk, std::placeholders::_1);

      if ((result = std::find_if(_chunks.begin(), _chunks.end(), cmpf)) !=
          _chunks.end())
        result->append(buff);
      else if ((result = std::find_if(_chunks.begin(), _chunks.end(), cmpo)) !=
          _chunks.end())
        result->merge(chunk, buff);
      else if ((result = std::find_if(_chunks.begin(), _chunks.end(), cmpl)) !=
          _chunks.end())
      {
        result->prepend(chunk, buff);
        result->remove();
        _chunks.erase(result);
        _chunks.push_back(chunk);
      }
      else
      {
        chunk.save(buff);
        _chunks.push_back(chunk);
      }

      return buff.size();
    }

    elle::Buffer
    Clerk::fetch(FileID id, Offset off, Size size)
    {
      if (not _identified)
        throw elle::Exception("Trying to fetch something without identifying");

      elle::Buffer ret;

      Chunk chunk(_base_path, id, off, size);
      std::vector<Chunk>::const_iterator result;
      auto cmpb = std::bind(&Chunk::belongs_to, &chunk, std::placeholders::_1);

      if ((result = std::find_if(_chunks.begin(), _chunks.end(), cmpb)) !=
          _chunks.end())
        return result->extract(chunk);

      throw elle::Exception("Chunk not found");
    }
  }
}
