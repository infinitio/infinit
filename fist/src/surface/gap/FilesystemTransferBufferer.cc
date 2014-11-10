#include <boost/filesystem/fstream.hpp>

#include <elle/serialize/PairSerializer.hxx>
#include <elle/serialize/VectorSerializer.hxx>
#include <elle/serialize/construct.hh>
#include <elle/serialize/extract.hh>
#include <elle/serialize/insert.hh>

#include <surface/gap/FilesystemTransferBufferer.hh>

ELLE_LOG_COMPONENT("surface.gap.FilesystemTransferBufferer");

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    FilesystemTransferBufferer::FilesystemTransferBufferer(
      infinit::oracles::PeerTransaction& transaction,
      boost::filesystem::path const& root):
      Super(transaction),
      _root(root / transaction.id),
      _count(),
      _full_size(),
      _files(),
      _key_code()
    {
      try
      {
        create_directories(this->_root);
        elle::serialize::from_file((this->_root / "count").string())
          >> this->_count;
        elle::serialize::from_file((this->_root / "total_size").string())
          >> this->_full_size;
        elle::serialize::from_file((this->_root / "files").string())
          >> this->_files;
        elle::serialize::from_file((this->_root / "key").string())
          >> this->_key_code;
      }
      catch(...)
      {
        throw DataExhausted();
      }
    }

    FilesystemTransferBufferer::FilesystemTransferBufferer(
      infinit::oracles::PeerTransaction& transaction,
      boost::filesystem::path const& root,
      FileCount count,
      FileSize full_size,
      std::vector<std::pair<std::string, FileSize>> const& files,
      infinit::cryptography::Code const& key):
      Super(transaction),
      _root(root / transaction.id),
      _count(count),
      _full_size(full_size),
      _files(files),
      _key_code(key)
    {
      create_directories(this->_root);
      {
        elle::serialize::to_file((this->_root / "count").string())
          << count;
      }
      {
        elle::serialize::to_file((this->_root / "total_size").string())
          << full_size;
      }
      {
        elle::serialize::to_file((this->_root / "files").string())
          << files;
      }
      {
        elle::serialize::to_file((this->_root / "key").string())
          << key;
      }
    }

    /*------.
    | Frete |
    `------*/

    // FilesystemTransferBufferer::FileCount
    // FilesystemTransferBufferer::count()
    // {

    // }

    // FilesystemTransferBufferer::FileSize
    // FilesystemTransferBufferer::full_size()
    // {

    // }

    std::vector<std::pair<std::string, FilesystemTransferBufferer::FileSize>>
    FilesystemTransferBufferer::files_info() const
    {
      return this->_files;
    }

    infinit::cryptography::Code
    FilesystemTransferBufferer::read(FileID f, FileOffset start, FileSize size)
    {
      elle::unreachable();
    }

    infinit::cryptography::Code
    FilesystemTransferBufferer::encrypted_read(FileID f,
                                               FileOffset start,
                                               FileSize)
    {
      return infinit::cryptography::Code(this->get(f, start));
    }

    // infinit::cryptography::Code const&
    // FilesystemTransferBufferer::key_code() const
    // {

    // }

    /*----------.
    | Buffering |
    `----------*/

    void
    FilesystemTransferBufferer::put(FileID file,
                                    FileOffset offset,
                                    FileSize size,
                                    elle::ConstWeakBuffer const& b)
    {
      auto filename = this->_filename(file, offset);
      boost::filesystem::ofstream output(filename);
      output.write(reinterpret_cast<const char*>(b.contents()), b.size());
      reactor::sleep(20_ms);
    }

    elle::Buffer
    FilesystemTransferBufferer::get(FileID file,
                                    FileOffset offset)
    {
      auto filename = this->_filename(file, offset);
      boost::filesystem::ifstream input(filename);
      input.seekg(0, std::ios::end);
      std::ios::pos_type size = input.tellg();
      if (size == std::ios::pos_type(-1))
      {
        ELLE_TRACE("Data exhausted on %s/%s at %s", file, offset, filename);
        throw DataExhausted();
      }
      elle::Buffer res(size);
      input.seekg(0, std::ios::beg);
      input.read(reinterpret_cast<char*>(res.mutable_contents()), size);
      reactor::sleep(20_ms);
      return res;
    }

    boost::filesystem::path
    FilesystemTransferBufferer::_filename(FileID file,
                                          FileOffset offset)
    {
      auto dir = this->_root / std::to_string(file);
      create_directory(dir);
      return dir / boost::filesystem::path(
        std::to_string(offset));
    }

    TransferBufferer::List
    FilesystemTransferBufferer::list()
    {
      return std::vector<std::pair<FileID, std::pair<FileSize, FileSize>>>();
    }

    /*----------.
    | Printable |
    `----------*/
    void
    FilesystemTransferBufferer::print(std::ostream& stream) const
    {
      stream << "FilesystemTransferBufferer (transaction_id: "
             << this->transaction().id << ")";
    }

    void
    FilesystemTransferBufferer::cleanup()
    {

    }
  }
}
