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
    _rpc_count("count", this->_rpc),
    _rpc_full_size("full_size", this->_rpc),
    _rpc_size_file("size", this->_rpc),
    _rpc_path("path", this->_rpc),
    _rpc_read("read", this->_rpc),
    _rpc_set_progress("progress", this->_rpc),
    _total_size(0),
    _progress(0.0f),
    _progress_changed("progress changed signal")
  {
    this->_rpc_count = std::bind(&Self::_count,
                                this);
    this->_rpc_full_size = std::bind(&Self::_full_size,
                                this);
    this->_rpc_size_file = std::bind(&Self::_file_size,
                                     this,
                                     std::placeholders::_1);
    this->_rpc_path = std::bind(&Self::_path,
                                this,
                                std::placeholders::_1);
    this->_rpc_read = std::bind(&Self::_read,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2,
                                std::placeholders::_3);
    this->_rpc_set_progress = std::bind(&Self::_set_progress,
                                        this,
                                        std::placeholders::_1);
  }

  Frete::~Frete()
  {}

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
        if (!boost::filesystem::is_directory(*it))
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
    this->_total_size += boost::filesystem::file_size(root / path);
    this->_paths.push_back(Path(root, path));
  }

  void
  Frete::get(boost::filesystem::path const& output_path)
  {
    uint64_t count = this->_rpc_count();
    this->_total_size = this->_rpc_full_size();

    ELLE_ASSERT_NEQ(this->_total_size, 0u);

    static std::streamsize N = 512 * 1024;
    for (uint64_t index = 0; index < count; ++index)
    {
      std::streamsize current_pos = 0;

      auto relativ_path = boost::filesystem::path{this->_rpc_path(index)};

      auto fullpath = output_path / relativ_path;

      // Create subdir.
      boost::filesystem::create_directories(fullpath.parent_path());

      std::ofstream output{fullpath.string()};

      while (true)
      {
        if (!output.good())
          throw elle::Exception("output is invalid");

        // Get the buffer from the rpc.
        elle::Buffer buffer{std::move(this->_rpc_read(index, current_pos, N))};

        // Write the file.
        output.write((char const*) buffer.mutable_contents(), buffer.size());

        if (!output.good())
          elle::Exception("writting let the stream not in a good state");

        current_pos += buffer.size();

        this->_increment_progress(buffer.size());

        if (buffer.size() < N)
        {
          output.close();
          break;
        }
      }
    }
  }

  void
  Frete::_increment_progress(uint64_t increment)
  {
    this->_progress += increment;
    this->_progress_changed.signal();
    this->_rpc_set_progress(this->_progress);
  }

  float
  Frete::progress() const
  {
    ELLE_ASSERT_NEQ(this->_total_size, 0u);
    return this->_progress / (float) this->_total_size;
  }

 /*-------------.
  | Remote calls |
  `-------------*/

  uint64_t
  Frete::count()
  {
    return this->_rpc_count();
  }

  uint64_t
  Frete::full_size()
  {
    return this->_rpc_full_size();
  }

  uint64_t
  Frete::file_size(FileID f)
  {
    return this->_rpc_size_file(f);
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

  boost::filesystem::path
  Frete::_local_path(FileID file_id)
  {
    return this->_paths[file_id].first / this->_paths[file_id].second;
  }

  uint64_t
  Frete::_count()
  {
    return this->_paths.size();
  }

  uint64_t
  Frete::_full_size()
  {
    return this->_total_size;
  }

  uint64_t
  Frete::_file_size(FileID file_id)
  {
    ELLE_ASSERT_LT(file_id, this->_count());
    return boost::filesystem::file_size(this->_local_path(file_id));
  }

  std::string
  Frete::_path(FileID file_id)
  {
    ELLE_ASSERT_LT(file_id, this->_count());
    return this->_paths[file_id].second.native();
  }

  elle::Buffer
  Frete::_read(FileID file_id,
               Offset offset,
               Size const size)
  {
    ELLE_TRACE_FUNCTION(file_id, offset, size);
    ELLE_ASSERT_LT(file_id, this->_count());
    auto path = this->_local_path(file_id);
    std::ifstream file(path.native());
    static const std::size_t MAX_offset{
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

  void
  Frete::_set_progress(uint64_t progress)
  {
    this->_progress = progress;
    this->_progress_changed.signal();
  }

  void
  Frete::run()
  {
    this->_rpc.run();
  }
}
