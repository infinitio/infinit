#include <hermes/Chunk.hh>

namespace oracle
{
  namespace hermes
  {
    // Used for building when storing a new independent chunk.
    Chunk::Chunk(boost::filesystem::path& base, FileID id, Offset off, Size s):
      _id(id),
      _off(off),
      _size(s)
    {
      _path = (base / _name()).replace_extension(".blk");
    }

    Chunk::Chunk(boost::filesystem::path const& path)
    {
      boost::filesystem::path tmp_path(path);
      std::string name(tmp_path.replace_extension().filename().string());

      std::vector<std::string> strings;
      boost::split(strings, name, boost::is_any_of("_"));

      try
      {
        _id = boost::lexical_cast<int>(strings[0]);
        _off = boost::lexical_cast<int>(strings[1]);
      }
      catch (boost::bad_lexical_cast const&)
      {
        throw elle::Exception("Invalid file in transaction folder");
      }

      _size = boost::filesystem::file_size(path);
    }

    bool
    Chunk::follows(Chunk const& other) const
    {
      if (_id != other._id)
        return false;
      return _off == (other._off + other._size);
    }

    bool
    Chunk::overlaps(Chunk const& other) const
    {
      if (_id != other._id)
        return false;
      return _off >= other._off and _off < (other._off + other._size);
    }

    bool
    Chunk::leads(Chunk const& other) const
    {
      if (_id != other._id)
        return false;
      return other._off == (_off + _size);
    }

    bool
    Chunk::belongs_to(Chunk const& oth) const
    {
      if (_id != oth._id)
        return false;
      return _off >= oth._off and _off <= (oth._off + oth._size);
    }

    void
    Chunk::append(elle::Buffer& buff)
    {
      std::ofstream out(_path.string(), std::fstream::app);
      if (not out.is_open())
        throw elle::Exception("IO error: could not save file");

      out.write(reinterpret_cast<char*>(buff.mutable_contents()), buff.size());
      _size += buff.size();

      out.close();
    }

    void
    Chunk::save(elle::Buffer& buff)
    {
      std::ofstream out(_path.string());
      if (not out.is_open())
        throw elle::Exception("IO error: could not save file");

      out.write(reinterpret_cast<char*>(buff.mutable_contents()), buff.size());
      _size = buff.size();

      out.close();
    }

    void
    Chunk::merge(Chunk const& other, elle::Buffer& buff)
    {
      // The function is called with this as the permanent chunk already stored
      // and other as the search block.
      if (_off < other._off)
      {
        std::ofstream out(_path.string(), std::fstream::app);
        if (not out.is_open())
          throw elle::Exception("IO error: could not save file");

        Offset offset(_off + _size - other._off);
        Size size(other._off + other._size - (_off + _size));

        out.write(reinterpret_cast<char*>(buff.mutable_contents()) + offset,
                  size);
        _size += size;

        out.close();
        return;
      }

      // In such a case, data is not sent in order. TODO
      throw elle::Exception("Transfert error: data was not sent in order");
    }

    void
    Chunk::prepend(Chunk& other, elle::Buffer& buff)
    {
      std::ofstream out(other._path.string());
      if (not out.is_open())
        throw elle::Exception("IO error: could not save file");

      out.write(reinterpret_cast<char*>(buff.mutable_contents()), buff.size());
      other._size = buff.size();

      Size file_size = boost::filesystem::file_size(_path);
      std::ifstream in(_path.string());

      char* buffer = new char[2048];
      while (not in.eof())
      {
        in.read(buffer, 2048);
        out.write(buffer, in.gcount());
      }
      delete[] buffer;

      in.close();
      other._size += file_size;

      out.close();
    }

    elle::Buffer
    Chunk::extract(Chunk const& piece) const
    {
      std::ifstream file(_path.string());
      elle::Buffer ret(_off + _size - piece._off);

      file.seekg(piece._off - _off);
      file.read(reinterpret_cast<char*>(ret.mutable_contents()), piece._size);

      return ret;
    }

    void
    Chunk::remove()
    {
      boost::filesystem::remove(_path);
    }

    std::string
    Chunk::_name()
    {
      return std::to_string(_id) + "_" + std::to_string(_off);
    }
  }
}
