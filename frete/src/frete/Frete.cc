#include <fstream>
#include <functional>
#include <ios>
#include <limits>

#include <frete/Frete.hh>

ELLE_LOG_COMPONENT("frete.Frete");

namespace frete
{
  /*-------------.
  | Construction |
  `-------------*/

  Frete::Frete(infinit::protocol::ChanneledStream& channels):
    _rpc(channels),
    _rpc_size("size", this->_rpc),
    _rpc_path("path", this->_rpc),
    _rpc_read("read", this->_rpc),
    _rpc_thread(*reactor::Scheduler::scheduler(),
                elle::sprintf("%s RPCS", *this),
                [this] { this->_rpc.run(); })
  {
    this->_rpc_size = std::bind(&Self::_size,
                                this);
    this->_rpc_path = std::bind(&Self::_path,
                                this,
                                std::placeholders::_1);
    this->_rpc_read = std::bind(&Self::_read,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);
  }

  Frete::~Frete()
  {
    this->_rpc_thread.terminate_now();
  }

  void
  Frete::add(boost::filesystem::path const& path)
  {
    if (is_directory(path))
    {
      boost::filesystem::recursive_directory_iterator end;
      for (auto it = boost::filesystem::recursive_directory_iterator(path);
           it != end;
           ++it)
      {
        auto parent = path.parent_path();
        boost::filesystem::path relative;
        auto rel = it->path().begin();
        for (auto count = parent.begin(); count != parent.end(); ++count)
          ++rel;
        for (; rel != it->path().end(); ++rel)
          relative /= *rel;
        this->add(parent, relative);
      }
    }
    else
      this->add(path.parent_path(), path.filename());
  }

  void
  Frete::add(boost::filesystem::path const& root,
             boost::filesystem::path const& path)
  {
    this->_paths.push_back(Path(root, path));
  }

  /*-------------.
  | Remote calls |
  `-------------*/

  uint64_t
  Frete::size()
  {
    return this->_rpc_size();
  }

  std::string
  Frete::path(FileID f)
  {
    return this->_rpc_path(f);
  }

  elle::Buffer
  Frete::read(FileID f, Offset start, Size size)
  {
    return this->_rpc_read(f, start, size);
  }

  /*-----.
  | RPCs |
  `-----*/

  uint64_t
  Frete::_size()
  {
    return this->_paths.size();
  }

  std::string
  Frete::_path(FileID file_id)
  {
    ELLE_ASSERT_LT(file_id, this->_size());
    return this->_paths[file_id].second.native();
  }

  elle::Buffer
  Frete::_read(FileID file_id,
               Offset offset,
               Size const size)
  {
    ELLE_TRACE_FUNCTION(file_id, offset, size);
    ELLE_ASSERT_LT(file_id, this->_size());
    auto path = this->_paths[file_id].first / this->_paths[file_id].second;
    std::ifstream file(path.native());
    static const std::streamsize MAX_offset{
      std::numeric_limits<std::streamsize>::max()};
    static const size_t MAX_buffer{elle::Buffer::max_size};

    if (size > MAX_buffer)
      throw elle::Exception(
        elle::sprintf("buffer that big (%s) can't be addressed", size));

    if (!file.good())
      throw elle::Exception("file is broken");

    // If the offset is greater than the machine maximum streamsize, seekg n
    // times to reach the right offset.
    while (offset > MAX_offset)
    {
      file.seekg(MAX_offset, std::ios_base::cur);

      if (!file.good())
        throw elle::Exception(
          elle::sprintf("unable to increment offset by %s", MAX_offset));

      offset -= MAX_offset;
    }

    file.seekg(offset, std::ios_base::cur);

    if (!file.good())
      throw elle::Exception(
        elle::sprintf("unable to seek to pos %s", offset));

    elle::Buffer buffer(size);

    buffer.size(
      file.readsome(reinterpret_cast<char*>(buffer.mutable_contents()),
                    size));

    if (file.fail())
      throw elle::Exception("unable to read");

    return buffer;
  }
}
