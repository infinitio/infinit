#include <surface/gap/PeerTransactionMachine.hh>

namespace surface
{
  namespace gap
  {
    PeerTransactionMachine::PeerTransactionMachine(Transaction& transaction,
                                                   uint32_t id,
                                                   std::shared_ptr<Data> data)
      : Super(transaction, id, data)
      , _data(data)
    {}
  }
}
