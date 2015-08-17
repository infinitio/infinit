#ifndef INFINIT_ORACLES_LINK_TRANSACTION_HH
# define INFINIT_ORACLES_LINK_TRANSACTION_HH

# include <vector>

# include <boost/optional.hpp>

# include <elle/serialization/fwd.hh>

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
      LinkTransaction(uint32_t click_count,
                      std::string cloud_location,
                      FileList file_list,
                      std::string hash,
                      std::string message,
                      std::string name,
                      std::string share_link,
                      boost::optional<bool> screenshot);
      LinkTransaction();
      ~LinkTransaction() noexcept(true);

    /*-----.
    | Data |
    `-----*/
    public:
      uint32_t click_count;
      std::string cloud_location;
      FileList file_list;
      std::string hash;
      std::string message;
      std::string name;
      std::string share_link;
      boost::optional<bool> screenshot;

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
