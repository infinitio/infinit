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
    /*-------------.
    | Construction |
    `-------------*/
    public:
      PeerTransaction();
      PeerTransaction(std::string sender_id,
                      std::string sender_fullname,
                      std::string sender_device_id,
                      std::string recipient_id);
      PeerTransaction(PeerTransaction&&) = default;
      PeerTransaction(PeerTransaction const&) = default;
      PeerTransaction&
      operator =(PeerTransaction const&) = default;
      ~PeerTransaction() noexcept(true);

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
      std::string download_link;
      int64_t total_size;

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
