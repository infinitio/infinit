#ifndef INFINIT_ORACLE_TRANSACTION_HH
# define INFINIT_ORACLE_TRANSACTION_HH

# include <elle/serialize/construct.hh>
# include <elle/Printable.hh>

# include <iosfwd>
# include <list>
# include <string>

namespace infinit
{
  namespace oracles
  {
    class Transaction:
      public elle::Printable
    {
    public:
      enum class Status: int
      {
# define TRANSACTION_STATUS(name, value)                                        \
        name = value,
# include <infinit/oracles/meta/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
      };

    public:
      Transaction() = default;
      Transaction(std::string const& sender_id,
                  std::string const& sender_fullname,
                  std::string const& sender_device_id);
      Transaction(Transaction&&) = default;
      Transaction(Transaction const&) = default;
      Transaction&
      operator =(Transaction const&) = default;

      ELLE_SERIALIZE_CONSTRUCT(Transaction)
      {}
      virtual
      ~Transaction() = default;

    public:
      std::string id;
      std::string sender_id;
      std::string sender_fullname;
      std::string sender_device_id;
      std::string recipient_id;
      std::string recipient_fullname;
      std::string recipient_device_id;
      std::string recipient_device_name;
      std::string message;
      std::list<std::string> files;
      uint32_t files_count;
      uint64_t total_size;
      bool is_directory;
      Status status;
      double ctime;
      double mtime;

      virtual
      void
      print(std::ostream& stream) const override;

      bool
      empty() const;
    };

    std::ostream&
    operator <<(std::ostream& out,
                Transaction::Status t);

    std::ostream&
    operator <<(std::ostream& out,
                Transaction const& t);
  }
}

# include <infinit/oracles/meta/Transaction.hxx>

#endif
