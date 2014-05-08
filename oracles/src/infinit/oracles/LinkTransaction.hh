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
    public:
      typedef std::pair<std::string, int64_t>  FileNameSizePair;
      typedef std::vector<FileNameSizePair> FileList;
    public:
      LinkTransaction();

    public:
      uint32_t click_count;
      std::string cloud_location;
      double expiry_time;
      FileList file_list;
      std::string hash;
      std::string name;

    // Serialization
    public:
      LinkTransaction(elle::serialization::SerializerIn& s);
      void
      serialize(elle::serialization::Serializer& s);
    };

    std::ostream&
    operator <<(std::ostream& out, LinkTransaction const& t);
  }
}

#endif
