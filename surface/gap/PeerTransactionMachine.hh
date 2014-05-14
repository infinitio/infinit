#ifndef SURFACE_GAP_PEER_TRANSACTION_MACHINE_HH
# define SURFACE_GAP_PEER_TRANSACTION_MACHINE_HH

# include <infinit/oracles/PeerTransaction.hh>
# include <surface/gap/TransactionMachine.hh>

namespace surface
{
  namespace gap
  {
    class PeerTransactionMachine:
      public TransactionMachine
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef infinit::oracles::PeerTransaction Data;
      typedef PeerTransactionMachine Self;
      typedef TransactionMachine Super;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      PeerTransactionMachine(Transaction& transaction,
                             uint32_t id,
                             std::shared_ptr<Data> data);
      ELLE_ATTRIBUTE_R(std::shared_ptr<Data>, data);
    };
  }
}

#endif
