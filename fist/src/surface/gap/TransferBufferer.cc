#include <surface/gap/TransferBufferer.hh>

#include <version.hh>

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

    TransferBufferer::DataExhausted::DataExhausted()
    : elle::Exception("Data exhausted")
    {}

    TransferBufferer::TransferBufferer(
      infinit::oracles::PeerTransaction& transaction):
      _transaction(transaction)
    {}

    void
    TransferBufferer::print(std::ostream& stream) const
    {
      stream << "TransferBufferer (transaction_id: " << this->transaction().id
             << ")";
    }

    /*------.
    | Frete |
    `------*/

    void
    TransferBufferer::set_progress(FileSize progress)
    {}

    void
    TransferBufferer::finish()
    {}

    TransferBufferer::FileSize
    TransferBufferer::file_size(FileID f)
    {
      return files_info()[f].second;
    }

    elle::Version
    TransferBufferer::version() const
    {
      return elle::Version(INFINIT_VERSION_MAJOR,
                           INFINIT_VERSION_MINOR,
                           INFINIT_VERSION_SUBMINOR);
    }

    TransferBufferer::TransferInfo
    TransferBufferer::transfer_info()
    {
      return TransferInfo{
        this->count(), this->full_size(), this->files_info()};
    }


    std::string
    TransferBufferer::path(FileID f)
    {
      return files_info()[f].first;
    }

    infinit::cryptography::Code
    TransferBufferer:: encrypted_read_acknowledge(FileID f, FileOffset start, FileSize size, FileSize progress)
    {
      set_progress(progress);
      return encrypted_read(f, start, size);
    }
  }
}
