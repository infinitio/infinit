#ifndef SURFACE_GAP_TRANSFER_BUFFERER_HH
# define SURFACE_GAP_TRANSFER_BUFFERER_HH

# include <elle/Printable.hh>

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
      typedef frete::Frete::FileSize FileOffset;
      typedef frete::Frete::FileSize FileSize;
      typedef
        std::vector<std::pair<FileID, std::pair<FileOffset, FileSize>>> List;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      TransferBufferer(infinit::oracles::Transaction& transaction);
      ELLE_ATTRIBUTE_R(infinit::oracles::Transaction&, transaction);

    /*----------.
    | Buffering |
    `----------*/
    public:
      virtual
      void
      put(FileID file,
          FileOffset offset,
          FileSize size,
          elle::ConstWeakBuffer const& b) = 0;
      virtual
      elle::Buffer
      get(FileID file,
          FileOffset offset) = 0;
      virtual
      List
      list() = 0;

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;
    };
  }
}

#endif
