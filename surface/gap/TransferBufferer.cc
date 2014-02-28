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
  }
}
