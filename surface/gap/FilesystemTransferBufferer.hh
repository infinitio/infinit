#ifndef SURFACE_GAP_FILESYSTEM_TRANSFER_BUFFERER_HH
# define SURFACE_GAP_FILESYSTEM_TRANSFER_BUFFERER_HH

# include <boost/filesystem/path.hpp>

# include <elle/attribute.hh>

# include <surface/gap/TransferBufferer.hh>

namespace surface
{
  namespace gap
  {
    class FilesystemTransferBufferer:
      public TransferBufferer
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef FilesystemTransferBufferer Self;
      typedef TransferBufferer Super;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      FilesystemTransferBufferer(infinit::oracles::Transaction& transaction,
                                 boost::filesystem::path const& root);
      ELLE_ATTRIBUTE_R(boost::filesystem::path, root);

    /*----------.
    | Buffering |
    `----------*/
    public:
      virtual
      void
      put(FileID file,
          FileOffset offset,
          FileSize size,
          elle::ConstWeakBuffer const& b) override;
      virtual
      elle::Buffer
      get(FileID file,
          FileOffset offset) override;
      virtual
      List
      list() override;

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
