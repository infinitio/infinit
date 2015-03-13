#include <frete/TransferSnapshot.hh>

#include <elle/log.hh>
#include <elle/serialization/SerializerIn.hh>
#include <elle/serialization/SerializerOut.hh>

ELLE_LOG_COMPONENT("frete.Snapshot");

namespace frete
{
  // Recipient.
  TransferSnapshot::TransferSnapshot(Frete::FileCount count,
                                     Frete::FileSize total_size,
                                     std::string const& relative_folder)
    : _count(count)
    , _total_size(total_size)
    , _progress(0)
    , _archived(false)
    , _mirrored(false)
    , _relative_folder(relative_folder)
  {}

  // Sender.
  TransferSnapshot::TransferSnapshot(bool mirrored)
    : _count(0)
    , _total_size(0)
    , _progress(0)
    , _archived(false)
    , _mirrored(mirrored)
    , _relative_folder()
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
    // size can be past end if the caller asks for a read past end.
    size = std::min(size, file._size);
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
    ELLE_DEBUG("Adding file %s/%s of size %s at index %s to snapshot",
               root, path, size, this->_count - 1);
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
    , _root(root.generic_string())
    , _path(path.generic_string())
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

  void
  TransferSnapshot::progress_increment(FileSize increment)
  {
    ELLE_DUMP_SCOPE("Incrementing progress of %s", increment);
    FileSize remain = increment;
    for (FileCount i = 0; i < count() && remain; ++i)
    {
      File& f = file(i);
      FileSize take = std::min(remain, f.size() - f.progress());
      ELLE_DUMP("Took %s from file %s at %s/%s", take, i, f.progress(), f.size());
      remain -= take;
      f._progress += take;
    }
    _progress += increment - remain;
    if (remain)
      throw elle::Exception(
        elle::sprintf("progress_increment(%s): could not increment of %s bytes with %s files",
                      increment, remain, file_count()));
  }

  void
  TransferSnapshot::set_key_code(infinit::cryptography::Code const& key_code)
  {
    if (_key_code)
      throw elle::Exception("Key code already set.");
    _key_code.reset(new infinit::cryptography::Code(key_code));
  }

  void
  TransferSnapshot::_recompute_progress()
  {
    _progress = 0;
    for (auto const& f: _files)
      _progress += f.second.progress();
  }

  /*--------------.
  | Serialization |
  `--------------*/

  TransferSnapshot::TransferSnapshot(elle::serialization::SerializerIn& input)
  {
    this->serialize(input);
  }

  void
  TransferSnapshot::serialize(elle::serialization::Serializer& s)
  {
    s.serialize("relative_folder", this->_relative_folder);
    s.serialize("mirrored", this->_mirrored);
    s.serialize("transfers", this->_files);
    s.serialize("count", this->_count);
    s.serialize("total_size", this->_total_size);
    s.serialize("progress", this->_progress);
    s.serialize("key_code", this->_key_code);
    s.serialize("archived", this->_archived);
    if (s.in())
      // Progress is redundant data, don't trust it
      this->_recompute_progress();
  }

  TransferSnapshot::File::File(elle::serialization::SerializerIn& input)
  {
    this->serialize(input);
  }

  void
  TransferSnapshot::File::serialize(elle::serialization::Serializer& s)
  {
    s.serialize("file_id", this->_file_id);
    s.serialize("root", this->_root);
    s.serialize("path", this->_path);
    s.serialize("file_size", this->_size);
    s.serialize("progress", this->_progress);
    if (s.in())
      this->_full_path = boost::filesystem::path(this->_root) / this->_path;
  }
}
