#ifndef INFINIT_ORACLES_LINK_TRANSACTION_HH
# define INFINIT_ORACLES_LINK_TRANSACTION_HH

# include <infinit/oracles/Transaction.hh>

# include <elle/serialization/fwd.hh>
# include <elle/serialize/construct.hh>

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
      LinkTransaction(std::string id,
                      std::string fullname,
                      std::string device_id,
                      std::vector<std::string> files);

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
      std::string share_link;

    /*--------------.
    | Serialization |
    `--------------*/
    public:
      LinkTransaction(elle::serialization::SerializerIn& s);
      virtual
      void
      serialize(elle::serialization::Serializer& s) override;

      ELLE_SERIALIZE_CONSTRUCT(LinkTransaction)
      {}

    /*----------.
    | Printable |
    `----------*/
    public:
      void
      print(std::ostream& out) const override;
    };
  }
}

# include <infinit/oracles/LinkTransaction.hxx>

#endif
