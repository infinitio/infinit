#ifndef  SURFACE_GAP_GAP_H
# define SURFACE_GAP_GAP_H

# include <stddef.h>

# ifdef __cplusplus
extern "C" {
# endif

  /// Status returned froym gap calls.
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

  typedef enum
  {
    gap_true = 1,
    gap_false = 0,
  } gap_Bool;

  /// gap_State is an opaque structure used in every calls.
  struct gap_State;
  typedef struct gap_State gap_State;

  // - gap ctor & dtor --------------------------------------------------------

  /// Create a new state.
  /// Returns NULL on failure.
  gap_State* gap_new();
  gap_State* gap_new_with_token(char const* token);

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

  gap_Status
  gap_notifications_read(gap_State*);

  //- Process interface -------------------------------------------------------

  /// Identifier for a process launch by gap.
  typedef int gap_ProcessId;

  /// Status of a process.
  typedef int gap_ProcessStatus;

  /// The process ended with an error.
  extern gap_ProcessStatus gap_process_status_failure;

  /// The process successfully ended.
  extern gap_ProcessStatus gap_process_status_success;

  /// The process is still running.
  extern gap_ProcessStatus gap_process_status_running;

  /// Returns the process status.
  gap_ProcessStatus
  gap_process_status(gap_State* state,
                     gap_ProcessId const pid);

  /// Try to finalize a process. Returns an error if the process
  /// does not exist, or if it's still running.
  gap_Status
  gap_process_finalize(gap_State* state,
                       gap_ProcessId const pid);

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
  int
  gap_logged_in(gap_State* state);

  /// Logout from meta.
  gap_Status gap_logout(gap_State* state);

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

  /// Login to trophonius
  gap_Status
  gap_trophonius_connect(gap_State* state);

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

  //- Transaction -------------------------------------------------------------

  typedef enum
  {
# define TRANSACTION_STATUS(name, value)        \
      gap_transaction_status_ ## name = value,
# include <oracle/disciples/meta/resources/transaction_status.hh.inc>
# undef TRANSACTION_STATUS
  } gap_TransactionStatus;

  /// New transaction callback.
  typedef void (*gap_transaction_callback_t)(char const* transaction_id,
                                             int is_new);
  gap_Status
  gap_transaction_callback(gap_State* state,
                           gap_transaction_callback_t cb);

  /// Transaction status callback.
  typedef void (*gap_transaction_status_callback_t)(char const*, int new_);
  gap_Status
  gap_transaction_status_callback(gap_State* state,
                                  gap_transaction_status_callback_t cb);

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

  // gap_Bool
  int
  gap_transaction_is_directory(gap_State*,
                               char const*);

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
  gap_Status gap_create_network(gap_State* state,
                                char const* name);

  /// Retrieve all user networks ids. Returned value is null in case of
  /// error, or is a null-terminated array of null-terminated strings.
  char** gap_networks(gap_State* state);

  /// Release the pointer returned by gap_networks,
  void gap_networks_free(char** networks);

  /// Get the network name from its id.
  char const* gap_network_name(gap_State* state, char const* id);

  /// Get the network mount point.
  char const* gap_network_mount_point(gap_State* state,
                                      char const* id);

  /// Retreive all users ids from a network. Returns a null-terminated array
  /// of null-terminated strings.
  char** gap_network_users(gap_State* state, char const* id);

  /// Free a previously allocated network users'.
  void gap_network_users_free(char** users);

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

  /// - User ------------------------------------------------------------------

  /// Retrieve user fullname.
  char const* gap_user_fullname(gap_State* state, char const* id);

  /// Retrieve user handle.
  char const* gap_user_handle(gap_State* state, char const* id);

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

  /// Change file permissions for a user.
  // XXX exec permission does not work
  gap_Status gap_set_permissions(gap_State* state,
                                 char const* user_id,
                                 char const* absolute_path,
                                 int permissions);

  /// Retrieve users associated to a file (have read and/or write access).
  char** gap_file_users(gap_State* state,
                        char const* absolute_path);

  void gap_file_users_free(char** users);

  /// Retrieve file permissions for a user. Returns -1 in case of errors.
  // XXX exec permission does not work
  int gap_get_permissions(gap_State* state,
                          char const* user_id,
                          char const* absolute_path);

  /// Get the list of transaction ids involving the user.
  char**
  gap_transactions(gap_State* state);

  /// Free transaction list.
  void
  gap_transactions_free(char** transactions);

  /// Send files to a specific user.
  ///
  /// @returns a unique identifier or -1 on error.
  gap_ProcessId
  gap_send_files(gap_State* state,
                 char const* recipient_id,
                 char const* const* files);


  /// Update transaction status.
  gap_Status
  gap_update_transaction(gap_State* state,
                         char const* transaction_id,
                         gap_TransactionStatus status);

  // Set output directory.
  gap_Status
  gap_set_output_dir(gap_State* state,
                     char const* output_path);

  char const*
  gap_get_output_dir(gap_State* state);

# ifdef __cplusplus
} // ! extern "C"
# endif

#endif
