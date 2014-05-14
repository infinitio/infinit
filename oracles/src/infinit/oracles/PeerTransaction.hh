#ifndef INFINIT_ORACLES_PEER_TRANSACTION_HH
# define INFINIT_ORACLES_PEER_TRANSACTION_HH

# include <infinit/oracles/Transaction.hh>

# include <elle/serialization/fwd.hh>
# include <elle/serialize/construct.hh>

namespace infinit
{
  namespace oracles
  {
    class PeerTransaction:
      public Transaction
    {
    public:
      PeerTransaction();
      PeerTransaction(std::string const& sender_id,
                      std::string const& sender_fullname,
                      std::string const& sender_device_id);
      PeerTransaction(PeerTransaction&&) = default;
      PeerTransaction(PeerTransaction const&) = default;
      PeerTransaction&
      operator =(PeerTransaction const&) = default;

    /*-----.
    | Data |
    `-----*/
    public:
      std::list<std::string> files;
      bool is_directory;
      int64_t files_count;
      std::string message;
      std::string recipient_id;
      std::string recipient_fullname;
      std::string recipient_device_id;
      std::string recipient_device_name;
      std::string sender_fullname;
      int64_t total_size;


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
      PeerTransaction(elle::serialization::SerializerIn& s);
      virtual
      void
      serialize(elle::serialization::Serializer& s) override;
      ELLE_SERIALIZE_CONSTRUCT(PeerTransaction)
      {}

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& out) const override;
    };
  }
}

# include <infinit/oracles/PeerTransaction.hxx>

#endif
