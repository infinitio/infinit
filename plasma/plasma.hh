#ifndef PLASMA_PLASMA_HH
# define PLASMA_PLASMA_HH

# include <elle/serialize/construct.hh>
# include <elle/Printable.hh>

# include <iosfwd>
# include <string>

namespace plasma
{

  enum class TransactionStatus: int
  {
# define TRANSACTION_STATUS(name, value)                                       \
    name = value,
# include <oracle/disciples/meta/src/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
  };

  struct Transaction:
    public elle::Printable
  {
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
    std::string network_id;
    std::string message;
    std::string first_filename;
    unsigned int files_count;
    uint64_t total_size;
    bool is_directory;
    TransactionStatus status;
    double timestamp;

    virtual
    void
    print(std::ostream& stream) const override;

    bool
    empty() const;
  };

  std::ostream&
  operator <<(std::ostream& out,
              TransactionStatus t);

  std::ostream&
  operator <<(std::ostream& out,
              Transaction const& t);
}

# include "plasma.hxx"

#endif
