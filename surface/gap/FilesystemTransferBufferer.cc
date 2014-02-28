#include <boost/filesystem/fstream.hpp>

#include <surface/gap/FilesystemTransferBufferer.hh>

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    FilesystemTransferBufferer::FilesystemTransferBufferer(
      infinit::oracles::Transaction& transaction,
      boost::filesystem::path const& root):
      Super(transaction),
      _root(root / transaction.id)
    {
      create_directories(this->_root);
    }

    /*----------.
    | Buffering |
    `----------*/

    void
    FilesystemTransferBufferer::put(FileID file,
                                    FileOffset offset,
                                    FileSize size,
                                    elle::ConstWeakBuffer const& b)
    {
      auto dir = this->_root / boost::lexical_cast<std::string>(file);
      create_directory(dir);
      auto filename =
        boost::lexical_cast<std::string>(offset) + "-" +
        boost::lexical_cast<std::string>(size);
      boost::filesystem::ofstream output(dir / filename);
      output.write(reinterpret_cast<const char*>(b.contents()), b.size());
    }

    elle::Buffer
    FilesystemTransferBufferer::get(FileID file,
                                    FileOffset offset)
    {
      return elle::Buffer();
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
  }
}
