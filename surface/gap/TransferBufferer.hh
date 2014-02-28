#ifndef SURFACE_GAP_TRANSFER_BUFFERER_HH
# define SURFACE_GAP_TRANSFER_BUFFERER_HH

# include <frete/Frete.hh>

# include <surface/gap/Transaction.hh>

namespace surface
{
  namespace gap
  {
    class TransferBufferer
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef TransferBufferer Self;
      typedef frete::Frete::FileID FileID;
      typedef frete::Frete::FileSize FileSize;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      TransferBufferer(infinit::oracles::Transaction& transaction);
      ELLE_ATTRIBUTE(infinit::oracles::Transaction&, transaction);

    /*----------.
    | Buffering |
    `----------*/
    public:
      virtual
      void
      put(FileID file,
          FileSize offset,
          FileSize size,
          elle::ConstWeakBuffer const& b) = 0;
      virtual
      elle::Buffer
      get(FileID file,
          FileSize offset) = 0;
      virtual
      std::vector<std::pair<FileID, std::pair<FileSize, FileSize>>>
      list() = 0;
    };
  }
}

#endif
