#include <frete/TransferSnapshot.hh>

ELLE_LOG_COMPONENT("frete.TransferSnapshot");

namespace frete
{
  TransferSnapshot::TransferInfo::TransferInfo(
    FileID file_id,
    boost::filesystem::path const& root,
    boost::filesystem::path const& path,
    Size file_size):
    _file_id(file_id),
    _root(root.string()),
    _path(path.string()),
    _full_path(root / path),
    _file_size(file_size)
  {}

  bool
  TransferSnapshot::TransferInfo::file_exists() const
  {
    return boost::filesystem::exists(this->_full_path);
  }

  bool
  TransferSnapshot::TransferInfo::operator ==(TransferInfo const& rh) const
  {
    return ((this->_file_id == rh._file_id) &&
            (this->_full_path == rh._full_path) &&
            (this->_file_size == rh._file_size));
  }

  void
  TransferSnapshot::TransferInfo::print(std::ostream& stream) const
  {
    stream << "TransferInfo "
           << this->_file_id << " : " << this->_full_path
           << "(" << this->_file_size << ")";
  }

  TransferSnapshot::TransferProgressInfo::TransferProgressInfo(
    FileID file_id,
    boost::filesystem::path const& root,
    boost::filesystem::path const& path,
    Size file_size):
    TransferInfo(file_id, root, path, file_size),
    _progress(0)
  {}

  void
  TransferSnapshot::TransferProgressInfo::_increment_progress(
    Size increment)
  {
    this->_progress += increment;
  }

  bool
  TransferSnapshot::TransferProgressInfo::complete() const
  {
    return (this->file_size() == this->_progress);
  }

  void
  TransferSnapshot::TransferProgressInfo::print(
    std::ostream& stream) const
  {
    stream << "ProgressInfo "
           << this->file_id() << " : " << this->full_path()
           << "(" << this->_progress << " / " << this->file_size() << ")";
  }

  bool
  TransferSnapshot::TransferProgressInfo::operator ==(
    TransferProgressInfo const& rh) const
  {
    return ((this->TransferInfo::operator ==(rh) &&
             this->_progress == rh._progress));
  }

  // Recipient.
  TransferSnapshot::TransferSnapshot(uint64_t count,
                                            Size total_size):
    _sender(false),
    _count(count),
    _total_size(total_size),
    _progress(0)
  {
    ELLE_LOG("%s: contruction", *this);
  }

  // Sender.
  TransferSnapshot::TransferSnapshot():
    _sender(true),
    _count(0),
    _total_size(0),
    _progress(0)
  {
    ELLE_LOG("%s: contruction", *this);
  }

  void
  TransferSnapshot::increment_progress(FileID index,
                                              Size increment)
  {
    ELLE_ASSERT_LT(index, this->_transfers.size());

    ELLE_TRACE("increment progress for file %s by %s", index, increment);

    ELLE_DEBUG("old progress was %s", this->_transfers.at(index).progress());
    this->_transfers.at(index)._increment_progress(increment);
    ELLE_DEBUG("new progress is %s", this->_transfers.at(index).progress());
    ELLE_DEBUG("old total progress was %s", this->progress());
    this->_progress += increment;
    ELLE_DEBUG("new total progress is %s", this->progress());
  }

  void
  TransferSnapshot::add(boost::filesystem::path const& root,
                               boost::filesystem::path const& path)
  {
    ELLE_ASSERT(this->_sender);

    auto file = root / path; if (!boost::filesystem::exists(file))
      throw elle::Exception(elle::sprintf("file %s doesn't exist", file));

    auto index = this->_transfers.size();
    auto size = boost::filesystem::file_size(file);
    this->_transfers.emplace(
      std::piecewise_construct,
      std::make_tuple(index),
      std::forward_as_tuple(index, root, path, size));
    this->_total_size += size;
    this->_count = this->_transfers.size();
  }

  bool
  TransferSnapshot::operator ==(TransferSnapshot const& rh) const
  {
    return ((this->_count == rh._count) &&
            (this->_total_size == rh._total_size) &&
            (this->_progress == rh._progress) &&
            std::equal(this->_transfers.begin(),
                       this->_transfers.end(),
                       rh._transfers.begin()));
  }

  void
  TransferSnapshot::progress(Size const& progress)
  {
    ELLE_ASSERT(this->sender());

    if (progress < this->_progress)
      ELLE_WARN("%s: reducing progress", *this);

    this->_progress = progress;
  }

  void
  TransferSnapshot::print(std::ostream& stream) const
  {
    stream << "Snapshot " << (this->_sender ? "sender" : "recipient")
           << " " << this->_count
           << " files for a total size of " << this->_total_size
           << ". Already 'copied': " << this->_progress << ": "
           << this->_transfers;
  }
}
