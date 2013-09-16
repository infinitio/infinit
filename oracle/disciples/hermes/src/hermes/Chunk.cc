#include <oracle/disciples/hermes/src/hermes/Chunk.hh>

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

    Chunk::Chunk(std::string const& config_line)
    {
      // TODO
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
      std::cout << _off << " " << _size << " " << other._off << " " << other._size  << " " << std::endl;

      if (_id != other._id)
        return false;
      return other._off == (_off + _size);
    }

    void
    Chunk::append(elle::Buffer& buff)
    {
      std::ofstream out(_path.string().c_str(), std::fstream::app);
      if (not out.is_open())
        throw elle::Exception("IO error: could not save file");

      out.write(reinterpret_cast<char*>(buff.mutable_contents()), buff.size());
      _size += buff.size();

      out.close();
    }

    void
    Chunk::save(elle::Buffer& buff)
    {
      std::ofstream out(_path.string().c_str());
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
        std::ofstream out(_path.string().c_str(), std::fstream::app);
        if (not out.is_open())
          throw elle::Exception("IO error: could not save file");

        Offset offset(_off + _size - other._off);
        Size size(other._off + other._size - (_off + _size));

        // TODO: Handle incoherences between files while merging?
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
      std::ofstream out(other._path.string().c_str());
      if (not out.is_open())
        throw elle::Exception("IO error: could not save file");

      out.write(reinterpret_cast<char*>(buff.mutable_contents()), buff.size());
      other._size = buff.size();

      Size file_size = boost::filesystem::file_size(_path);
      std::ifstream in(_path.string().c_str());
      out << in.rdbuf();
      in.close();

      other._size += file_size;

      out.close();
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
