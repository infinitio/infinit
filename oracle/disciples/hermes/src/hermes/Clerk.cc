#include <oracle/disciples/hermes/src/hermes/Clerk.hh>
// TODO: Correct include path.
// TODO: learn to pass exact exceptions through network

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

    }

    Size
    Clerk::store(FileID id, Offset off, elle::Buffer& buff)
    {
      if (not _identified)
        throw elle::Exception("Trying to store something without identifying");

      Chunk chunk(_base_path, id, off, buff.size());

      // TODO: Modify names of this stuff.
      std::vector<Chunk>::iterator result;
      auto follows = std::bind(&Chunk::follows, &chunk, std::placeholders::_1);
      auto overlaps = std::bind(&Chunk::overlaps,
                                &chunk,
                                std::placeholders::_1);

      auto leads = std::bind(&Chunk::leads, &chunk, std::placeholders::_1);

      std::cout << "list state " << _chunks.size() << std::endl;

      if ((result = std::find_if(_chunks.begin(), _chunks.end(), follows)) !=
          _chunks.end())
      {
        std::cout << "follows" << std::endl;
        result->append(buff);
      }
      else if ((result = std::find_if(_chunks.begin(),
                                      _chunks.end(),
                                      overlaps)) != _chunks.end())
      {
        std::cout << "overlaps" << std::endl;
        result->merge(chunk, buff);
      }
      else if ((result = std::find_if(_chunks.begin(),
                                      _chunks.end(),
                                      leads)) != _chunks.end())
      {
        // TODO: coded too fast, memory problems.
        std::cout << "leads" << std::endl;
        result->prepend(chunk, buff);

        result->remove();
        _chunks.erase(result);

        _chunks.push_back(chunk);
      }
      else
      {
        std::cout << "save" << std::endl;
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

      return elle::Buffer(0);
    }

    void
    Clerk::ident(TID id)
    {
      // TODO: Check stuff with Transaction ID.

      boost::filesystem::path relative_path(id);
      _explore(relative_path);

      _identified = true;
    }
  }
}
