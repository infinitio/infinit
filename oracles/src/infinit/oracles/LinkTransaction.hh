#ifndef INFINIT_ORACLES_LINK_TRANSACTION_HH
# define INFINIT_ORACLES_LINK_TRANSACTION_HH

# include <vector>

# include <boost/optional.hpp>

# include <elle/serialization/fwd.hh>
# include <elle/serialize/construct.hh>

# include <infinit/oracles/Transaction.hh>

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
      ~LinkTransaction() noexcept(true);

    /*-----.
    | Data |
    `-----*/
    public:
      uint32_t click_count;
      std::string cloud_location;
      FileList file_list;
      std::string message;
      std::string name;
      std::string share_link;

    public:
      uint64_t
      size() const;
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
