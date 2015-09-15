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
      typedef std::vector<std::pair<std::string, FileSize>> Files;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Recipient constructor.
      FilesystemTransferBufferer(infinit::oracles::PeerTransaction& transaction,
                                 boost::filesystem::path const& root);
      /// Sender constructor.
      FilesystemTransferBufferer(infinit::oracles::PeerTransaction& transaction,
                                 boost::filesystem::path const& root,
                                 FileCount count,
                                 FileSize total_size,
                                 Files const& files,
                                 infinit::cryptography::Code const& key);
      ELLE_ATTRIBUTE_R(boost::filesystem::path, root);
      ELLE_ATTRIBUTE_R(FileCount, count);
      ELLE_ATTRIBUTE_R(FileSize, full_size);
      ELLE_ATTRIBUTE_R(Files, files);
      ELLE_ATTRIBUTE_R(infinit::cryptography::Code, key_code);

    /*------.
    | Frete |
    `------*/
    public:
      // /// Return the number of file.
      // virtual
      // FileCount
      // count() override;
      // /// Return the total size of files.
      // virtual
      // FileSize
      // full_size() override;
      /// Return the path and size of all files.
      virtual
      std::vector<std::pair<std::string, FileSize>>
      files_info() const override;
      /// Return a weakly crypted chunk of a file.
      virtual
      infinit::cryptography::Code
      read(FileID f, FileOffset start, FileSize size) override;
      /// Return a strongly crypted chunk of a file.
      virtual
      infinit::cryptography::Code
      encrypted_read(FileID f, FileOffset start, FileSize size) override;
      // /// Get the key of the transfer.
      // virtual
      // infinit::cryptography::Code const&
      // key_code() const override;

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
      virtual
      void
      cleanup() override;
    private:
      boost::filesystem::path
      _filename(FileID file,
                FileOffset offset);

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
