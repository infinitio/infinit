#ifndef FRETE_TRANSFERSNAPSHOT_HH
# define FRETE_TRANSFERSNAPSHOT_HH

# include <boost/filesystem.hpp>

# include <elle/serialize/BinaryArchive.hh>
# include <elle/Buffer.hh>
# include <elle/container/map.hh>

# include <frete/Types.hh>

namespace frete
{
  struct TransferSnapshot:
    public elle::Printable
  {
    // Recipient.
    TransferSnapshot(uint64_t count,
                      Size total_size);

    // Sender.
    TransferSnapshot();

    void
    progress(Size const& progress);

    void
    increment_progress(FileID index,
                        Size increment);

    void
    add(boost::filesystem::path const& root,
        boost::filesystem::path const& path);

    struct TransferInfo:
      public elle::Printable
    {
      TransferInfo(FileID file_id,
                    boost::filesystem::path const& root,
                    boost::filesystem::path const& path,
                    Size file_size);

      virtual
      ~TransferInfo() = default;

      ELLE_ATTRIBUTE_R(FileID, file_id);

      ELLE_ATTRIBUTE(std::string, root);
      ELLE_ATTRIBUTE_R(std::string, path);

      ELLE_ATTRIBUTE_R(boost::filesystem::path, full_path);
      ELLE_ATTRIBUTE_R(Size, file_size);

      bool
      file_exists() const;

      bool
      operator ==(TransferInfo const& rh) const;

      /*----------.
      | Printable |
      `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;

      /*--------------.
      | Serialization |
      `--------------*/

      // XXX: Serialization require a default constructor when serializing
      // pairs...
      TransferInfo() = default;


      ELLE_SERIALIZE_CONSTRUCT(TransferInfo)
      {}

      ELLE_SERIALIZE_FRIEND_FOR(TransferInfo);

    };

    struct TransferProgressInfo:
      public TransferInfo
    {
    public:
      TransferProgressInfo(FileID file_id,
                            boost::filesystem::path const& root,
                            boost::filesystem::path const& path,
                            Size file_size);

      void
      update_progress(Size progress);

      ELLE_ATTRIBUTE_R(Size, progress);

    private:
      void
      _increment_progress(Size increment);

    public:
      bool
      complete() const;

      bool
      operator ==(TransferProgressInfo const& rh) const;

      friend TransferSnapshot;

      /*----------.
      | Printable |
      `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const;

      /*--------------.
      | Serialization |
      `--------------*/

      TransferProgressInfo() = default;

      ELLE_SERIALIZE_CONSTRUCT(TransferProgressInfo, TransferInfo)
      {}

      ELLE_SERIALIZE_FRIEND_FOR(TransferProgressInfo);
    };

    ELLE_ATTRIBUTE_R(bool, sender);
    typedef std::unordered_map<FileID, TransferProgressInfo> TransferProgress;
    ELLE_ATTRIBUTE_X(TransferProgress, transfers);
    ELLE_ATTRIBUTE_R(uint64_t, count);
    ELLE_ATTRIBUTE_R(Size, total_size);
    ELLE_ATTRIBUTE_R(Size, progress);

    bool
    operator ==(TransferSnapshot const& rh) const;

    /*----------.
    | Printable |
    `----------*/
  public:
    virtual
    void
    print(std::ostream& stream) const;

    /*--------------.
    | Serialization |
    `--------------*/

    ELLE_SERIALIZE_CONSTRUCT(TransferSnapshot)
    {}

    ELLE_SERIALIZE_FRIEND_FOR(TransferSnapshot);
  };
}

#include <frete/TransferSnapshot.hxx>

#endif // !FRETE_TRANSFERSNAPSHOT_HH
