#include <surface/gap/TransferBufferer.hh>

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
