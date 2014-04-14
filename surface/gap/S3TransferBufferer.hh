#ifndef SURFACE_GAP_S3_TRANSFER_BUFFERER_HH
# define SURFACE_GAP_S3_TRANSFER_BUFFERER_HH

# include <boost/filesystem/path.hpp>

# include <elle/attribute.hh>
# include <elle/json/json.hh>

# include <surface/gap/TransferBufferer.hh>

# include <aws/Credentials.hh>
# include <aws/S3.hh>

namespace surface
{
  namespace gap
  {
    class S3TransferBufferer:
      public TransferBufferer
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef S3TransferBufferer Self;
      typedef TransferBufferer Super;
      typedef std::vector<std::pair<std::string, FileSize>> Files;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      /// Recipient constructor.
      /// The recipient fetches the meta-data from the cloud.
      S3TransferBufferer(
        infinit::oracles::Transaction& transaction,
        std::function<aws::Credentials(bool)> credentials,
        std::string const& bucket_name = "us-east-1-buffer-infinit-io");

      /// Sender constructor.
      /// The sender saves the meta-data for the transfer to the cloud.
      S3TransferBufferer(
        infinit::oracles::Transaction& transaction,
        std::function<aws::Credentials(bool)> credentials,
        FileCount count,
        FileSize total_size,
        Files const& files,
        infinit::cryptography::Code const& key,
        std::string const& bucket_name = "us-east-1-buffer-infinit-io");

      ELLE_ATTRIBUTE_R(FileCount, count);
      ELLE_ATTRIBUTE_R(FileSize, full_size);
      ELLE_ATTRIBUTE_R(Files, files);
      ELLE_ATTRIBUTE_R(infinit::cryptography::Code, key_code);

    /*------.
    | Frete |
    `------*/
    public:
      /// Return the path and size of all files.
      virtual
      std::vector<std::pair<std::string, FileSize>>
      files_info() const override;
      /// Return a weakly crypted chunk of data.
      virtual
      infinit::cryptography::Code
      read(FileID f, FileOffset start, FileSize size) override;
      /// Return a strongly crypted chunk of data.
      virtual
      infinit::cryptography::Code
      encrypted_read(FileID f, FileOffset start, FileSize size) override;

    /*----------.
    | Buffering |
    `----------*/
    public:
      virtual
      void
      put(FileID file,
          FileSize offset,
          FileSize size,
          elle::ConstWeakBuffer const& b) override;
      virtual
      elle::Buffer
      get(FileID file,
          FileSize offset) override;
      virtual
      List
      list() override;
      virtual
      void
      cleanup() override;

    /*-----------.
    | Attributes |
    `-----------*/
    private:
      ELLE_ATTRIBUTE(std::string, bucket_name);
      ELLE_ATTRIBUTE(std::function<aws::Credentials(bool)>, credentials);
      ELLE_ATTRIBUTE(std::string, remote_folder);
      ELLE_ATTRIBUTE(aws::S3, s3_handler);

    /*--------.
    | Helpers |
    `--------.*/
    private:
    FileOffset
    _offset_from_s3_name(std::string const& s3_name);

    FileID
    _file_id_from_s3_name(std::string const& s3_name);

    std::string
    _make_s3_name(FileID const& file, FileOffset const& offset);

    List
    _convert_list(aws::S3::List const& list);

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
