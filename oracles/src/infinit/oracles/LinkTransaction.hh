#ifndef INFINIT_ORACLES_LINK_TRANSACTION_HH
# define INFINIT_ORACLES_LINK_TRANSACTION_HH

# include <infinit/oracles/Transaction.hh>

# include <elle/serialization/fwd.hh>

# include <vector>

namespace infinit
{
  namespace oracles
  {
    class LinkTransaction:
      public Transaction
    {
    /*------.
    | Types |
    `------*/
    public:
      typedef std::pair<std::string, int64_t>  FileNameSizePair;
      typedef std::vector<FileNameSizePair> FileList;

    /*-------------.
    | Construction |
    `-------------*/
    public:
      LinkTransaction();

    /*-----.
    | Data |
    `-----*/
    public:
      uint32_t click_count;
      std::string cloud_location;
      double expiry_time;
      FileList file_list;
      std::string hash;
      std::string name;

    /*-----------.
    | Properties |
    `-----------*/
    public:
      virtual
      bool
      concern_user(std::string const& user_id) const override;
      virtual
      bool
      concern_device(std::string const& user_id,
                     std::string const& device_id) const override;

    /*--------------.
    | Serialization |
    `--------------*/
    public:
      LinkTransaction(elle::serialization::SerializerIn& s);
      virtual
      void
      serialize(elle::serialization::Serializer& s) override;

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& out) const override;
    };
  }
}

#endif
