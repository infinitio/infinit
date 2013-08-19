#ifndef SURFACE_GAP_ENUMS_HH
# define SURFACE_GAP_ENUMS_HH

// This are a shared enums between C++ and C.

extern "C"
{
  typedef enum
  {
    TransferState_NewTransaction = 0,
    TransferState_SenderCreateNetwork = 1,
    TransferState_SenderCreateTransaction = 2,
    TransferState_SenderCopyFiles = 3,
    TransferState_SenderWaitForDecision = 4,
    TransferState_RecipientWaitForDecision = 5,
    TransferState_RecipientAccepted = 6,
    TransferState_GrantPermissions = 7,
    TransferState_PublishInterfaces = 8,
    TransferState_Connect = 9,
    TransferState_PeerDisconnected = 10,
    TransferState_PeerConnectionLost = 11,
    TransferState_Transfer = 12,
    TransferState_CleanLocal = 13,
    TransferState_CleanRemote = 14,
    TransferState_Finished = 15,
    TransferState_Rejected = 16,
    TransferState_Canceled = 17,
    TransferState_Failed = 18,
  } TransferState;

  typedef enum
  {
    NotificationType_NewTransaction,
    NotificationType_TransactionUpdate,
    NotificationType_UserStatusUpdate,
    NotificationType_NewSwagger,
    NotificationType_ConnectionStatus,
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
# define ERR_CODE(name, value, comment)                                         \
    gap_ ## name = value,
# include <oracle/disciples/meta/src/meta/error_code.hh.inc>
# undef ERR_CODE
  } gap_Status;

  //- Transaction -------------------------------------------------------------
  typedef enum
  {
# define TRANSACTION_STATUS(name, value)                                        \
    gap_transaction_status_ ## name = value,
# include <oracle/disciples/meta/src/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
  } gap_TransactionStatus;
}

#endif
