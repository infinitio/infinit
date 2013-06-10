#ifndef PLASMA_PLASMA_HH
# define PLASMA_PLASMA_HH

# include <elle/serialize/construct.hh>

# include <iosfwd>
# include <string>

namespace plasma
{

  enum class TransactionStatus: int
  {
# define TRANSACTION_STATUS(name, value)                                       \
    name = value,
# include <oracle/disciples/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
  };

  struct Transaction
  {
  public:
    Transaction();
    ELLE_SERIALIZE_CONSTRUCT(Transaction)
    {}
    virtual
    ~Transaction();

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
    int files_count;
    int total_size;
    bool is_directory;
    TransactionStatus status;
    bool accepted;
    double timestamp;
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
