#include <elle/log.hh>

#include <frete/TransferSnapshot.hh>

ELLE_LOG_COMPONENT("frete.Snapshot");

namespace frete
{
  // Recipient.
  TransferSnapshot::TransferSnapshot(Frete::FileCount count,
                                     Frete::FileSize total_size):
    _count(count),
    _total_size(total_size),
    _progress(0)
  {}

  // Sender.
  TransferSnapshot::TransferSnapshot():
    _count(0),
    _total_size(0),
    _progress(0)
  {}

  void
  TransferSnapshot::file_progress_increment(FileID file_id,
                                            FileSize increment)
  {
    ELLE_TRACE("%s: increment progress by %s", *this, increment);
    auto& file = this->file(file_id);
    file._progress += increment;
    ELLE_ASSERT_LTE(file._progress, file._size);
    this->_progress += increment;
  }

  void
  TransferSnapshot::file_progress_set(FileID file_id,
                                      FileSize size)
  {
    ELLE_TRACE("%s: set progress to %s", *this, size);
    auto& file = this->file(file_id);
    ELLE_ASSERT_LTE(size, file._size);
    auto increment = size - file._progress;
    this->file_progress_increment(file_id, increment);
  }

  void
  TransferSnapshot::file_progress_end(Frete::FileID file_id)
  {
    auto& file = this->file(file_id);
    this->file_progress_set(file_id, file._size);
  }

  /*------.
  | Files |
  `------*/

  void
  TransferSnapshot::add(boost::filesystem::path const& root,
                        boost::filesystem::path const& path)
  {
    auto file = root / path;
    if (!boost::filesystem::exists(file))
      throw elle::Exception(elle::sprintf("file %s doesn't exist", file));

    auto index = this->_files.size();
    auto size = boost::filesystem::file_size(file);
    this->_files.insert(std::make_pair(index, File(index, root, path, size)));
    this->_total_size += size;
    this->_count = this->_files.size();
  }

  TransferSnapshot::File&
  TransferSnapshot::file(FileID file_id)
  {
    try
    {
      return this->_files.at(file_id);
    }
    catch (std::exception const&)
    {
      throw elle::Exception(elle::sprintf("file id out of range: %s", file_id));
    }
  }

  bool
  TransferSnapshot::has(FileID file_id) const
  {
    return this->_files.find(file_id) != this->_files.end();
  }

  void
  TransferSnapshot::add(FileID file_id,
                        boost::filesystem::path const& root,
                        boost::filesystem::path const& path,
                        FileSize size)
  {
    this->_files.insert(
      std::make_pair(file_id, File(file_id, root, path, size)));
  }

  TransferSnapshot::File const&
  TransferSnapshot::file(FileID file_id) const
  {
    return const_cast<TransferSnapshot*>(this)->file(file_id);
  }

  bool
  TransferSnapshot::operator ==(TransferSnapshot const& rh) const
  {
    return ((this->_count == rh._count) &&
            (this->_total_size == rh._total_size) &&
            (this->_progress == rh._progress) &&
            std::equal(this->_files.begin(),
                       this->_files.end(),
                       rh._files.begin()));
  }

  void
  TransferSnapshot::progress(Frete::FileSize const& progress)
  {
    if (progress < this->_progress)
      ELLE_WARN("%s: reducing progress", *this);

    this->_progress = progress;
  }

  void
  TransferSnapshot::print(std::ostream& stream) const
  {
    elle::fprintf(stream, "Snapshot(%s files, %s bytes, %.2f%%)",
                  this->_count,
                  this->_total_size,
                  this->_progress / (float)this->_total_size);
  }

  /*---------.
  | Snapshot |
  `---------*/

  bool
  TransferSnapshot::File::file_exists() const
  {
    return boost::filesystem::exists(this->_full_path);
  }

  TransferSnapshot::File::File(Frete::FileID file_id,
                               boost::filesystem::path const& root,
                               boost::filesystem::path const& path,
                               Frete::FileSize size)
    : _file_id(file_id)
    , _root(root.string())
    , _path(path.string())
    , _full_path(root / path)
    , _size(size)
    , _progress(0)
  {}

  bool
  TransferSnapshot::File::complete() const
  {
    return this->_size == this->_progress;
  }

  void
  TransferSnapshot::File::print(std::ostream& stream) const
  {
    elle::fprintf(stream, "Snapshot::File(%s, %s, %.2f%%)",
                  this->file_id(),
                  this->full_path(),
                  this->_progress / (float)this->size());
  }

  bool
  TransferSnapshot::File::operator ==(File const& rh) const
  {
    return ((this->_file_id == rh._file_id) &&
            (this->_full_path == rh._full_path) &&
            (this->_size == rh._size));
  }

  TransferSnapshot::FileCount
  TransferSnapshot::file_count() const
  {
    return this->_files.size();
  }
}
