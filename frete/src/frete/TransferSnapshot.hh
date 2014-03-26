#ifndef FRETE_TRANSFERSNAPSHOT_HH
# define FRETE_TRANSFERSNAPSHOT_HH

# include <boost/filesystem.hpp>

# include <elle/Printable.hh>

# include <frete/Frete.hh>

namespace frete
{
  class TransferSnapshot:
    public elle::Printable
  {
  public:
    // Recipient.
    TransferSnapshot(Frete::FileCount count,
                     Frete::FileSize total_size);
    // Sender.
    TransferSnapshot();

  public:
    void
    progress(Frete::FileSize const& progress);
    void
    increment_progress(Frete::FileID index,
                       Frete::FileSize increment);
    void
    set_progress(Frete::FileID index,
                 Frete::FileSize progress);
    void
    end_progress(Frete::FileID index);
    void
    add(boost::filesystem::path const& root,
        boost::filesystem::path const& path);

  public:
    struct TransferProgressInfo:
      public elle::Printable
    {
    public:
      TransferProgressInfo(Frete::FileID file_id,
                           boost::filesystem::path const& root,
                           boost::filesystem::path const& path,
                           Frete::FileSize file_size);
      bool
      file_exists() const;

    private:
      void
      _increment_progress(Frete::FileSize increment);
      void
      _set_progress(Frete::FileSize progress);

    public:
      bool
      complete() const;

      bool
      operator ==(TransferProgressInfo const& rh) const;

      ELLE_ATTRIBUTE_R(Frete::FileID, file_id);
      ELLE_ATTRIBUTE(std::string, root);
      ELLE_ATTRIBUTE_R(std::string, path);
      ELLE_ATTRIBUTE_R(boost::filesystem::path, full_path);
      ELLE_ATTRIBUTE_R(Frete::FileSize, file_size);
      ELLE_ATTRIBUTE_R(Frete::FileSize, progress);

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
    public:
      TransferProgressInfo() = default;

      ELLE_SERIALIZE_CONSTRUCT(TransferProgressInfo)
      {}

      ELLE_SERIALIZE_FRIEND_FOR(TransferProgressInfo);
    };

    ELLE_ATTRIBUTE_R(bool, sender);
    typedef std::unordered_map<Frete::FileID, TransferProgressInfo> TransferProgress;
    ELLE_ATTRIBUTE_X(TransferProgress, transfers);
    ELLE_ATTRIBUTE_R(Frete::FileCount, count);
    ELLE_ATTRIBUTE_R(Frete::FileSize, total_size);
    ELLE_ATTRIBUTE_R(Frete::FileSize, progress);

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
  public:
    ELLE_SERIALIZE_CONSTRUCT(TransferSnapshot)
    {}

    ELLE_SERIALIZE_FRIEND_FOR(TransferSnapshot);
  };
}

# include <frete/TransferSnapshot.hxx>

#endif
