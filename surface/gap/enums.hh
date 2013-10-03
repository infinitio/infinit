#ifndef SURFACE_GAP_ENUMS_HH
# define SURFACE_GAP_ENUMS_HH

// This are a shared enums between C++ and C.
# ifdef __cplusplus
extern "C"
{
# endif
  typedef enum
  {
    gap_transaction_none,
    gap_transaction_pending,
    gap_transaction_copying,
    gap_transaction_waiting_for_accept,
    gap_transaction_accepted,
    gap_transaction_preparing,
    gap_transaction_running,
    gap_transaction_cleaning,
    gap_transaction_finished,
    gap_transaction_failed,
    gap_transaction_canceled,
    gap_transaction_rejected,
  } gap_TransactionStatus;

  typedef enum
  {
    NotificationType_NewTransaction,
    NotificationType_TransactionUpdate,
    NotificationType_UserStatusUpdate,
    NotificationType_NewSwagger,
    NotificationType_ConnectionStatus,
    NotificationType_KickedOut,
  } NotificationType;

  typedef enum
  {
    gap_ok = 1,
    gap_error = 0,
    gap_network_error = -2,
    gap_internal_error = -3,
    gap_no_device_error = -4,
    gap_wrong_passport = -5,
    gap_no_file = -6,
    gap_file_not_found = -7,
    gap_api_error = -10,
    gap_peer_to_peer_error = -11,
# define ERR_CODE(name, value, comment)                                         \
    gap_ ## name = value,
# include <oracle/disciples/meta/src/meta/error_code.hh.inc>
# undef ERR_CODE
  } gap_Status;

# ifdef __cplusplus
}
# endif

#endif
