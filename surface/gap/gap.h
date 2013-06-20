#ifndef  SURFACE_GAP_GAP_H
# define SURFACE_GAP_GAP_H

# include <stddef.h>

# ifdef __cplusplus
extern "C" {
# endif

#include <surface/gap/status.hh>

  //typedef enum
  //{
  //  gap_true = 1,
  //  gap_false = 0,
  //} gap_Bool;
  typedef int gap_Bool; // XXX Use the previous enum.

  /// gap_State is an opaque structure used in every calls.
  struct gap_State;
  typedef struct gap_State gap_State;

  // - gap ctor & dtor --------------------------------------------------------

  /// Create a new state.
  /// Returns NULL on failure.
  gap_State* gap_new();

  /// Release a state.
  void gap_free(gap_State* state);

  /// Enable debug messages.
  void gap_enable_debug(gap_State* state);

  /// Debug func.
  gap_Status gap_debug(gap_State* state);

  // - Services status --------------------------------------------------------

  /// Check if meta is alive.
  gap_Status gap_meta_status(gap_State* state);

  /// The root url of meta server.
  char const*
  gap_meta_url(gap_State* state);

  /// Debug func: Pull notifications.
  gap_Status
  gap_pull_notifications(gap_State*,
                         int count,
                         int offset);


  /// Debug func: Pull notifications.
  gap_Status
  gap_pull_new_notifications(gap_State*,
                             int count,
                             int offset);

  gap_Status
  gap_notifications_read(gap_State*);

  //- Operation interface -------------------------------------------------------

  /// Identifier for an operation launched by gap.
  typedef int gap_OperationId;

  /// Status of an operation.
  typedef int gap_OperationStatus;

  /// The operation ended with an error.
  extern gap_OperationStatus gap_operation_status_failure;

  /// The operation successfully ended.
  extern gap_OperationStatus gap_operation_status_success;

  /// The operation is still running.
  extern gap_OperationStatus gap_operation_status_running;

  /// Returns the operation status.
  gap_OperationStatus
  gap_operation_status(gap_State* state,
                     gap_OperationId const pid);

  /// Try to finalize an operation. Returns an error if the operation does not
  /// exist, or if it's still running.
  gap_Status
  gap_operation_finalize(gap_State* state,
                       gap_OperationId const pid);

  //- Authentication & registration -------------------------------------------

  /// Generate a hash for the password.
  /// NOTE: You are responsible to free the returned pointer with
  /// gap_hash_free.
  char* gap_hash_password(gap_State* state,
                          char const* email,
                          char const* password);

  /// Free a previously allocated hash.
  void gap_hash_free(char* h);

  /// Login to meta.
  gap_Status gap_login(gap_State* state,
                       char const* email,
                       char const* hash_password);

  /// Check is user is already logged.
  gap_Bool
  gap_logged_in(gap_State* state);

  /// Logout from meta.
  gap_Status gap_logout(gap_State* state);

  /// Get the current token, if any.
  gap_Status
  gap_token(gap_State* state, char** token);

  gap_Status
  gap_generation_key(gap_State* state, char** token);

  /// @brief Register to meta.
  ///
  /// If the device name is not NULL, it will also create
  /// the local device with specified name. The password hash is obtained via
  /// gap_hash_password() function.
  gap_Status gap_register(gap_State* state,
                          char const* fullname,
                          char const* email,
                          char const* hash_password,
                          char const* device_name,
                          char const* activation_code);

  //- Swaggers ----------------------------------------------------------------

  typedef enum
  {
    gap_user_status_offline = 0,
    gap_user_status_online = 1,
    gap_user_status_busy = 2,
  } gap_UserStatus;

  typedef void (*gap_user_status_callback_t)(char const*,
                                             gap_UserStatus const);

  gap_Status
  gap_user_status_callback(gap_State* state,
                           gap_user_status_callback_t cb);

  /// New transaction callback.
  typedef void (*gap_transaction_callback_t)(char const* transaction_id,
                                             int is_new);
  gap_Status
  gap_transaction_callback(gap_State* state,
                           gap_transaction_callback_t cb);

  /// Transaction getters.
  char const*
  gap_transaction_sender_id(gap_State*,
                            char const*);

  char const*
  gap_transaction_sender_fullname(gap_State*,
                                  char const*);

  char const*
  gap_transaction_sender_device_id(gap_State*,
                                   char const*);

  char const*
  gap_transaction_recipient_id(gap_State*,
                               char const*);

  char const*
  gap_transaction_recipient_fullname(gap_State*,
                                     char const*);

  char const*
  gap_transaction_recipient_device_id(gap_State*,
                                      char const*);

  char const*
  gap_transaction_network_id(gap_State*,
                             char const*);

  char const*
  gap_transaction_first_filename(gap_State*,
                                 char const*);

  int
  gap_transaction_files_count(gap_State*,
                              char const*);

  int
  gap_transaction_total_size(gap_State*,
                             char const*);

  double
  gap_transaction_timestamp(gap_State* state,
                            char const* transaction_id);

  gap_Bool
  gap_transaction_is_directory(gap_State*,
                               char const*);

  gap_Bool
  gap_transaction_accepted(gap_State* state,
                           char const* transaction_id);

  gap_TransactionStatus
  gap_transaction_status(gap_State*,
                         char const*);

  char const*
  gap_transaction_message(gap_State*,
                          char const*);

  float
  gap_transaction_progress(gap_State* state,
                           char const* transaction_id);

  /// Force transaction to be fetched again from server.
  gap_Status
  gap_transaction_sync(gap_State* state,
                       char const* transaction_id);

  /// - Message ---------------------------------------------------------------
  gap_Status
  gap_message(gap_State* state,
              char const* recipient_id,
              char const* message);

  typedef void (*gap_message_callback_t)(char const* sender_id, char const* message);
  gap_Status
  gap_message_callback(gap_State* state,
                       gap_message_callback_t cb);

  /// Poll
  gap_Status
  gap_poll(gap_State* state);


  gap_Status
  gap_invite_user(gap_State* state,
                  char const* email);

  /// - Device ----------------------------------------------------------------

  /// Returns the local device status.
  gap_Status gap_device_status(gap_State* state);

  /// Update the local device name.
  gap_Status gap_set_device_name(gap_State* state,
                                 char const* name);

  /// - Network -------------------------------------------------------------

  /// Create a new network.
  char const*
  gap_create_network(gap_State* state,
                     char const* name);

  /// Prepare a network.
  gap_Status
  gap_prepare_network(gap_State* state,
                      char const* network_id);

  /// Retrieve all user networks ids. Returned value is null in case of
  /// error, or is a null-terminated array of null-terminated strings.
  char** gap_networks(gap_State* state);

  /// Release the pointer returned by gap_networks,
  void gap_networks_free(char** networks);

  /// Get the network name from its id.
  char const* gap_network_name(gap_State* state, char const* id);

  /// Invite a user to join a network with its id or email.
  gap_Status gap_network_add_user(gap_State* state,
                                  char const* network_id,
                                  char const* user_id);

  /// - Self ------------------------------------------------------------------

  /// Get current user token.
  char const*
  gap_user_token(gap_State* state);

  /// Get current user email.
  char const*
  gap_self_email(gap_State* state);

  /// Get current user id.
  char const*
  gap_self_id(gap_State* state);

  /// Get current user remaining invitations.
  int
  gap_self_remaining_invitations(gap_State* state);

  /// - User ------------------------------------------------------------------

  /// Retrieve user fullname.
  char const* gap_user_fullname(gap_State* state, char const* id);

  /// Retrieve user handle.
  char const* gap_user_handle(gap_State* state, char const* id);

  // The user directory
  char const*
  gap_user_directory(gap_State* state, char const** directory);

  /// Retrieve user status.
  gap_UserStatus
  gap_user_status(gap_State* state, char const* user_id);

  /// @brief Retrieve user icon from a user_id
  /// @note data with be freed with gap_user_icon_free when the call is
  /// successfull.
  gap_Status
  gap_user_icon(gap_State* state,
                char const* user_id,
                void** data,
                size_t* size);

  /// Free a previously allocated user icon.
  void gap_user_icon_free(void* data);

  /// Retrieve user with its email.
  char const* gap_user_by_email(gap_State* state, char const* email);

  /// Search users.
  char** gap_search_users(gap_State* state, char const* text);

  /// Free the search users result.
  void gap_search_users_free(char** users);

  /// - Swaggers --------------------------------------------------------------

  /// Get the list of user's swaggers.
  char**
  gap_swaggers(gap_State* state);

  /// Free swagger list.
  void
  gap_swaggers_free(char** swaggers);

  /// - Permissions ---------------------------------------------------------

  typedef enum gap_Permission
  {
    gap_none  = 0,
    gap_read  = 1,
    gap_write = 2,
    gap_exec  = 4,
    // WARNING: negative values are reserved for errors, no value of this
    // enum should have a negative value.
  } gap_Permission;

  /// Get the list of transaction ids involving the user.
  char**
  gap_transactions(gap_State* state);

  /// Free transaction list.
  void
  gap_transactions_free(char** transactions);

  /// Error callback type. The arguments are respectively error code, reason
  /// and optionally a transaction id.
  typedef void (*gap_on_error_callback_t)(gap_Status status,
                                          char const* reason,
                                          char const* transaction_id);

  /// Register an error callback.
  gap_Status
  gap_on_error_callback(gap_State* state,
                        gap_on_error_callback_t cb);

  /// Send files to a specific user.
  ///
  /// @returns a unique identifier or -1 on error.
  gap_OperationId
  gap_send_files(gap_State* state,
                 char const* recipient_id,
                 char const* const* files);


  /// Update transaction status.
  gap_Status
  gap_update_transaction(gap_State* state,
                         char const* transaction_id,
                         gap_TransactionStatus status);

  /// Accept a transaction.
  /// This function can only be used by the recipient of the transaction, if
  /// not already accepted.
  gap_Status
  gap_accept_transaction(gap_State* state,
                         char const* transaction_id);

  // Set output directory.
  gap_Status
  gap_set_output_dir(gap_State* state,
                     char const* output_path);

  char const*
  gap_get_output_dir(gap_State* state);

  void
  gap_send_file_crash_report(char const* module,
                             char const* filename);

  gap_Status
  gap_gather_crash_reports(char const* user_id,
                           char const* network_id);

  // Generated file.
  #include <surface/gap/gen_metrics.h>

# ifdef __cplusplus
} // ! extern "C"
# endif

#endif
