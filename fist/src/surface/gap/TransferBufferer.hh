#ifndef SURFACE_GAP_TRANSFER_BUFFERER_HH
# define SURFACE_GAP_TRANSFER_BUFFERER_HH

# include <elle/Printable.hh>

# include <frete/Frete.hh>

# include <infinit/oracles/PeerTransaction.hh>
# include <surface/gap/Transaction.hh>

namespace surface
{
  namespace gap
  {
    /** Base class for providers of transfer buffers.
    * Transfer bufferers do not track trasnfer progress, they just
    * store data chunks on behalf or senders/recipients.
    */
    class TransferBufferer
    {
    /*------.
    | Types |
    `------*/
    public:

      class DataExhausted:
        public elle::Exception
      {
      public:
        DataExhausted();
      };

      typedef TransferBufferer Self;
      typedef frete::Frete::FileID FileID;
      typedef frete::Frete::TransferInfo TransferInfo;
      typedef frete::Frete::FileCount FileCount;
      typedef frete::Frete::FileOffset FileOffset;
      typedef frete::Frete::FileSize FileSize;
      typedef
        std::vector<std::pair<FileID, std::pair<FileOffset, FileSize>>> List;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      TransferBufferer(infinit::oracles::PeerTransaction& transaction);
      ELLE_ATTRIBUTE_R(infinit::oracles::PeerTransaction&, transaction);

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
      /// Return the path and size of all files.
      virtual
      std::vector<std::pair<std::string, FileSize>>
      files_info() const = 0;
      // For backward compatibility
      FileSize
      file_size(FileID f);
      // For backward compatibility
      std::string
      path(FileID f);
      /// Return a weakly crypted chunck of a file.
      virtual
      infinit::cryptography::Code
      read(FileID f, FileOffset start, FileSize size) = 0;
      /// Return a strongly crypted chunck of a file.
      virtual
      infinit::cryptography::Code
      encrypted_read(FileID f, FileOffset start, FileSize size) = 0;
      virtual
      infinit::cryptography::Code
      encrypted_read_acknowledge(FileID f, FileOffset start, FileSize size, FileSize progress);
      /// Get the key of the transfer.
      virtual
      infinit::cryptography::Code const&
      key_code() const = 0;
      /// Get all the info about the transfer.
      TransferInfo
      transfer_info();
      /// Update the progress. no-op.
      void
      set_progress(FileSize progress);
      /// Signal we're done
      void
      finish();
      /// Version.
      elle::Version
      version() const;

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
      // Request to clear buffered data when transfer is finished.
      virtual
      void
      cleanup() = 0;
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
