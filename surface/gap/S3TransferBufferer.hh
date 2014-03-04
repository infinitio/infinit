#ifndef SURFACE_GAP_S3_TRANSFER_BUFFERER_HH
# define SURFACE_GAP_S3_TRANSFER_BUFFERER_HH

# include <boost/filesystem/path.hpp>

# include <elle/attribute.hh>

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

    /*-------------.
    | Construction |
    `-------------*/
    public:
      S3TransferBufferer(
        infinit::oracles::Transaction& transaction,
        aws::Credentials const& credentials,
        std::string const& bucket_name = "us-east-1-buffer-infinit-io");

    /*----------.
    | Buffering |
    `----------*/
    public:
      virtual
      void
      put(FileID file,
          FileSize offset,
          FileSize size,
          elle::ConstWeakBuffer const& b);
      virtual
      elle::Buffer
      get(FileID file,
          FileSize offset);
      virtual
      List
      list();

    /*-----------.
    | Attributes |
    `-----------*/
    private:
      ELLE_ATTRIBUTE(std::string, bucket_name);
      ELLE_ATTRIBUTE(aws::Credentials, credentials);
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
