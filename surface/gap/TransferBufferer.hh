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
      typedef frete::Frete::FileCount FileCount;
      typedef frete::Frete::FileOffset FileOffset;
      typedef frete::Frete::FileSize FileSize;
      typedef
        std::vector<std::pair<FileID, std::pair<FileOffset, FileSize>>> List;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      TransferBufferer(infinit::oracles::Transaction& transaction);
      ELLE_ATTRIBUTE_R(infinit::oracles::Transaction&, transaction);

    /*------.
    | Frete |
    `------*/
    public:
      /// Return the number of file.
      virtual
      FileCount
      count() const = 0;
      /// Return the total size of files.
      virtual
      FileSize
      full_size() const = 0;
      /// Return the size of a file.
      virtual
      FileSize
      file_size(FileID f) const = 0;
      /// Return the path of a file.
      virtual
      std::string
      path(FileID f) const = 0;
      /// Return a weakly crypted chunck of a file.
      virtual
      infinit::cryptography::Code
      read(FileID f, FileOffset start, FileSize size) = 0;
      /// Return a strongly crypted chunck of a file.
      virtual
      infinit::cryptography::Code
      encrypted_read(FileID f, FileOffset start, FileSize size) = 0;
      /// Get the key of the transfer.
      virtual
      infinit::cryptography::Code const&
      key_code() const = 0;
      /// Update the progress.
      void
      set_progress(FileSize progress);
      /// Signal we're done
      void
      finish();

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
