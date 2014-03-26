#include <elle/log.hh>

#include <frete/TransferSnapshot.hh>

ELLE_LOG_COMPONENT("frete.Snapshot");

namespace frete
{
  // Recipient.
  TransferSnapshot::TransferSnapshot(Frete::FileCount count,
                                     Frete::FileSize total_size):
    _sender(false),
    _count(count),
    _total_size(total_size),
    _progress(0)
  {}

  // Sender.
  TransferSnapshot::TransferSnapshot():
    _sender(true),
    _count(0),
    _total_size(0),
    _progress(0)
  {}

  void
  TransferSnapshot::increment_progress(Frete::FileID index,
                                       Frete::FileSize increment)
  {
    ELLE_ASSERT_LT(index, this->_transfers.size());

    ELLE_TRACE("increment progress for file %s by %s", index, increment);

    ELLE_DUMP("old progress was %s", this->_transfers.at(index).progress());
    this->_transfers.at(index)._increment_progress(increment);
    ELLE_DEBUG("new progress is %s", this->_transfers.at(index).progress());
    ELLE_DUMP("old total progress was %s", this->progress());
    this->_progress += increment;
    ELLE_DEBUG("new total progress is %s", this->progress());
  }

  void
  TransferSnapshot::set_progress(Frete::FileID index,
                                 Frete::FileSize progress)
  {
    ELLE_ASSERT_LT(index, this->_transfers.size());
    ELLE_TRACE_SCOPE("%s: set progress for file %s to %s",
                     *this, index, progress);
    auto& transfer = this->_transfers.at(index);
    auto old = transfer.progress();
    if (old >= progress)
      return;
    transfer._set_progress(progress);
    this->_progress += progress - old;
  }

  void
  TransferSnapshot::end_progress(Frete::FileID index)
  {
    ELLE_ASSERT_LT(index, this->_transfers.size());
    ELLE_TRACE_SCOPE("%s: end progress for file %s",
                     *this, index);
    auto& transfer = this->_transfers.at(index);
    auto old = transfer.progress();
    auto progress = transfer.file_size();
    transfer._set_progress(progress);
    this->_progress += progress - old;
  }

  void
  TransferSnapshot::add(boost::filesystem::path const& root,
                        boost::filesystem::path const& path)
  {
    ELLE_ASSERT(this->_sender);

    auto file = root / path;
    if (!boost::filesystem::exists(file))
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
  TransferSnapshot::progress(Frete::FileSize const& progress)
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

  /*---------.
  | Snapshot |
  `---------*/

  bool
  TransferSnapshot::TransferProgressInfo::file_exists() const
  {
    return boost::filesystem::exists(this->_full_path);
  }

  TransferSnapshot::TransferProgressInfo::TransferProgressInfo(
    Frete::FileID file_id,
    boost::filesystem::path const& root,
    boost::filesystem::path const& path,
    Frete::FileSize file_size):
    _file_id(file_id),
    _root(root.string()),
    _path(path.string()),
    _full_path(root / path),
    _file_size(file_size),
    _progress(0)
  {}

  void
  TransferSnapshot::TransferProgressInfo::_increment_progress(
    Frete::FileSize increment)
  {
    this->_progress += increment;
  }

  void
  TransferSnapshot::TransferProgressInfo::_set_progress(
    Frete::FileSize progress)
  {
    this->_progress = progress;
  }

  bool
  TransferSnapshot::TransferProgressInfo::complete() const
  {
    return (this->file_size() == this->_progress);
  }

  void
  TransferSnapshot::TransferProgressInfo::print(std::ostream& stream) const
  {
    stream << "TransferProgressInfo "
           << this->file_id() << " : " << this->full_path()
           << "(" << this->_progress << " / " << this->file_size() << ")";
  }

  bool
  TransferSnapshot::TransferProgressInfo::operator ==(
    TransferProgressInfo const& rh) const
  {
    return ((this->_file_id == rh._file_id) &&
            (this->_full_path == rh._full_path) &&
            (this->_file_size == rh._file_size));
  }
}
