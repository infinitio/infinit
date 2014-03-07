#include <surface/gap/TransferBufferer.hh>

namespace surface
{
  namespace gap
  {
    /*-------------.
    | Construction |
    `-------------*/

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
  }
}
