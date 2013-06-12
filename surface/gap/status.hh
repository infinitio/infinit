#ifndef SURFACE_GAP_STATUS_HH
# define SURFACE_GAP_STATUS_HH

typedef enum
{
  gap_ok = 0,
  gap_error = -1,
  gap_network_error = -2,
  gap_internal_error = -3,
  gap_no_device_error = -4,
  gap_wrong_passport = -5,
  gap_no_file = -6,
  gap_file_not_found = -7,
  gap_api_error = -10,
# define ERR_CODE(name, value, comment)                                         \
  gap_ ## name = value,
# include <oracle/disciples/meta/error_code.hh.inc>
# undef ERR_CODE
} gap_Status;

  //- Transaction -------------------------------------------------------------
typedef enum
{
# define TRANSACTION_STATUS(name, value)                                        \
  gap_transaction_status_ ## name = value,
# include <oracle/disciples/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
} gap_TransactionStatus;

#endif
