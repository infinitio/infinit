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
      infinit::oracles::Transaction& transaction):
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

    infinit::cryptography::Code
    TransferBufferer:: encrypted_read_acknowledge(FileID f, FileOffset start, FileSize size, FileSize progress)
    {
      set_progress(progress);
      return encrypted_read(f, start, size);
    }
  }
}
