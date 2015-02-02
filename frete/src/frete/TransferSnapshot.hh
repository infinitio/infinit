#ifndef FRETE_TRANSFERSNAPSHOT_HH
# define FRETE_TRANSFERSNAPSHOT_HH

# include <boost/filesystem.hpp>

# include <elle/Printable.hh>
# include <elle/serialization/fwd.hh>

# include <frete/Frete.hh>

namespace frete
{
  class TransferSnapshot:
    public elle::Printable
  {
  /*------.
  | Types |
  `------*/
  public:
    typedef TransferSnapshot Self;
    typedef Frete::FileCount FileCount;
    typedef Frete::FileID FileID;
    typedef Frete::FileSize FileSize;

  /*-------------.
  | Construction |
  `-------------*/
  public:
    // Construct recipient snapshot.
    TransferSnapshot(FileCount count,
                     FileSize total_size,
                     std::string const& relative_folder = "");
    /// Construct sender snapshot.
    TransferSnapshot(bool mirrored);

  /*-----.
  | File |
  `-----*/
  public:
    struct File:
      public elle::Printable
    {
    /*-------------.
    | Construction |
    `-------------*/
    private:
      File(FileID file_id,
           boost::filesystem::path const& root,
           boost::filesystem::path const& path,
           FileSize size);
      friend TransferSnapshot;
    public:
      ELLE_ATTRIBUTE_R(FileID, file_id);
      ELLE_ATTRIBUTE(std::string, root);
      ELLE_ATTRIBUTE_R(std::string, path);
      ELLE_ATTRIBUTE_R(boost::filesystem::path, full_path);
      /// Total file size
      ELLE_ATTRIBUTE_R(FileSize, size);

    /*-------.
    | Status |
    `-------*/
    public:
      bool
      file_exists() const;
      bool
      complete() const;

    /*---------.
    | Progress |
    `---------*/
    public:
      /// Current file size or amount transmitted (depending if sender/recipient)
      ELLE_ATTRIBUTE_R(FileSize, progress);

    /*-----------.
    | Comparison |
    `-----------*/
    public:
      bool
      operator ==(File const& rh) const;

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
      File() = default;
      ELLE_SERIALIZE_CONSTRUCT(File)
      {}
      ELLE_SERIALIZE_FRIEND_FOR(File);

    public:
      File(elle::serialization::SerializerIn& input);
      void
      serialize(elle::serialization::Serializer& s);
    };

  /*------.
  | Files |
  `------*/
  public:
    void
    add(boost::filesystem::path const& root,
        boost::filesystem::path const& path);
    File&
    file(FileID file_id);
    File const&
    file(FileID file_id) const;
    bool
    has(FileID file_id) const;
    void
    add(FileID file_id,
        boost::filesystem::path const& root,
        boost::filesystem::path const& path,
        FileSize size);
    /// Set encryption symetric key, encrypted with current user's public key
    /// Can only be called once.
    void
    set_key_code(infinit::cryptography::Code const& code);
  private:
    typedef std::unordered_map<FileID, File> Files;
    ELLE_ATTRIBUTE_R(Files, files);
    ELLE_ATTRIBUTE_R(std::unique_ptr<infinit::cryptography::Code>, key_code);

  /*-----------.
  | Attributes |
  `-----------*/
  public:
    /// Total number of files.
    ELLE_ATTRIBUTE_R(FileCount, count);
    ELLE_ATTRIBUTE_R(FileSize, total_size);
    /// Number of files present locally (with index 0 to file_count()).
    FileCount file_count() const;
  /*---------.
  | Progress |
  `---------*/
  public:
    void
    file_progress_increment(FileID file, FileSize increment);
    void
    file_progress_set(FileID file, FileSize progress);
    void
    file_progress_end(FileID file);
    // Increment progress and appropriate file(s) progress of 'increment' bytes.
    void
    progress_increment(FileSize increment);
    // does not update individual files progress!
    void
    progress(FileSize const& progress);
    ELLE_ATTRIBUTE_R(FileSize, progress);

    // If the ghost cloud buffering archive has been fully archived.
    ELLE_ATTRIBUTE_RW(bool, archived);
    /// If the files were mirrored.
    ELLE_ATTRIBUTE_R(bool, mirrored);
    /// Folder relative to the root where files should be downloaded.
    /// Used in iOS as each transaction is saved in a folder.
    ELLE_ATTRIBUTE_R(std::string, relative_folder);
  /*-----------.
  | Comparison |
  `-----------*/
  public:
    bool
    operator ==(TransferSnapshot const& rh) const;

  /*----------.
  | Printable |
  `----------*/
  public:
    virtual
    void
    print(std::ostream& stream) const;

  private:
    // recalculate global progress from individual file data
    void _recompute_progress();
  /*--------------.
  | Serialization |
  `--------------*/
  public:
    ELLE_SERIALIZE_CONSTRUCT(TransferSnapshot)
    {}
    ELLE_SERIALIZE_FRIEND_FOR(TransferSnapshot);

  public:
    TransferSnapshot(elle::serialization::SerializerIn& input);
    void
    serialize(elle::serialization::Serializer& s);
  };
}

# include <frete/TransferSnapshot.hxx>

#endif
