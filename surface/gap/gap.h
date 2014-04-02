#ifndef  SURFACE_GAP_GAP_H
# define SURFACE_GAP_GAP_H

// XXX Used to ensure that generated gap.h in Obj-C doesn't contain system
// XXX declarations
# ifndef SURFACE_GAP_NO_SYSTEM_INCLUDE
#  include <stddef.h>
#  include <stdint.h>
# endif

# ifdef __cplusplus
extern "C" {
# endif

# include <surface/gap/enums.hh>

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

  /// Create a new state.
  /// Returns NULL on failure.
  gap_State* gap_configurable_new(char const* meta_protocol,
                                  char const* meta_host,
                                  unsigned short meta_port,
                                  char const* trophonius_host,
                                  unsigned short trophonius_port);

  /// Release a state.
  void gap_free(gap_State* state);

  /// Callback to be executed if an exception escape from the scheduler.
  typedef void (*gap_critical_callback_t)(char const*);

  gap_Status
  gap_critical_callback(gap_State* state,
                        gap_critical_callback_t cb);


  /// The 'error' value of ids.
  uint32_t
  gap_null();

  void
  gap_test(gap_State* state, char* string);

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

  //- Authentication & registration -------------------------------------------

  /// Generate a hash for the password.
  /// NOTE: You are responsible to free the returned pointer with
  /// gap_hash_free.
  char* gap_hash_password(gap_State* state,
                          char const* email,
                          char const* password);

  /// Free a previously allocated hash.
  void gap_hash_free(char* h);

  /// Fetch Meta message.
  /// In some cases when Meta is down, it will have an associated message.
  char const*
  gap_meta_down_message(gap_State*);

  /// Login to meta.
  gap_Status
  gap_login(gap_State* state,
            char const* email,
            char const* hash_password);

  /// Check is user is already logged.
  gap_Bool
  gap_logged_in(gap_State* state);

  /// Logout from meta.
  gap_Status
  gap_logout(gap_State* state);

  /// @brief Register to meta.
  ///
  /// If the device name is not NULL, it will also create
  /// the local device with specified name. The password hash is obtained via
  /// gap_hash_password() function.
  gap_Status
  gap_register(gap_State* state,
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

  typedef void (*gap_new_swagger_callback_t)(uint32_t id);

  gap_Status
  gap_new_swagger_callback(gap_State* state,
                           gap_new_swagger_callback_t cb);

  typedef void (*gap_user_status_callback_t)(uint32_t id,
                                             gap_UserStatus const);
  gap_Status
  gap_user_status_callback(gap_State* state,
                           gap_user_status_callback_t cb);

  typedef void (*gap_avatar_available_callback_t)(uint32_t id);
  gap_Status
  gap_avatar_available_callback(gap_State* state,
                                gap_avatar_available_callback_t cb);

  // Own connection status changed.
  typedef void (*gap_connection_callback_t)(gap_UserStatus const);

  gap_Status
  gap_connection_callback(gap_State* state,
                          gap_connection_callback_t cb);


  // Kicked out callback.
  // Triggered when your credentials are no longer valid.
  typedef void (*gap_kicked_out_callback_t)();

  gap_Status
  gap_kicked_out_callback(gap_State* state,
                          gap_kicked_out_callback_t cb);

  /// Trophonius unavailable callback.
  /// Triggered when you can connect to Meta but not to Trophonius.
  typedef void (*gap_trophonius_unavailable_callback_t)();

  gap_Status
  gap_trophonius_unavailable_callback(gap_State* state,
                                      gap_trophonius_unavailable_callback_t cb);

  /// New transaction callback.
  typedef void (*gap_transaction_callback_t)(uint32_t id,
                                             gap_TransactionStatus status);

  gap_Status
  gap_transaction_callback(gap_State* state,
                           gap_transaction_callback_t cb);

  /// Transaction getters.
  uint32_t
  gap_transaction_sender_id(gap_State*,
                            uint32_t);

  char const*
  gap_transaction_sender_fullname(gap_State*,
                                  uint32_t);

  char const*
  gap_transaction_sender_device_id(gap_State*,
                                   uint32_t);

  uint32_t
  gap_transaction_recipient_id(gap_State*,
                               uint32_t);

  char const*
  gap_transaction_recipient_fullname(gap_State*,
                                     uint32_t);

  char const*
  gap_transaction_recipient_device_id(gap_State*,
                                      uint32_t);

  char const*
  gap_transaction_network_id(gap_State*,
                             uint32_t);

  char**
  gap_transaction_files(gap_State* state,
                        uint32_t const transaction_id);

  int64_t
  gap_transaction_files_count(gap_State*,
                              uint32_t);

  int64_t
  gap_transaction_total_size(gap_State*,
                             uint32_t);

  double
  gap_transaction_ctime(gap_State* state,
                        uint32_t);

  double
  gap_transaction_mtime(gap_State* state,
                        uint32_t);

  gap_Bool
  gap_transaction_is_directory(gap_State*,
                               uint32_t);

  gap_TransactionStatus
  gap_transaction_status(gap_State* state,
                         uint32_t const);

  char const*
  gap_transaction_message(gap_State*,
                          uint32_t);

  float
  gap_transaction_progress(gap_State* state,
                           uint32_t);

  /* /// Force transaction to be fetched again from server. */
  /* gap_Status */
  /* gap_transaction_sync(gap_State* state, */
  /*                      char const* transaction_id); */

  /// - Message ---------------------------------------------------------------

  typedef void (*gap_message_callback_t)(char const* sender_id, char const* message);
  gap_Status
  gap_message_callback(gap_State* state,
                       gap_message_callback_t cb);

  /// Poll
  gap_Status
  gap_poll(gap_State* state);

  /// - Device ----------------------------------------------------------------

  /// Returns the local device status.
  gap_Status gap_device_status(gap_State* state);

  /// Update the local device name.
  gap_Status gap_set_device_name(gap_State* state,
                                 char const* name);

  /// - Self ------------------------------------------------------------------
  /// Get current user email.
  char const*
  gap_self_email(gap_State* state);

  /// Get current user id.
  uint32_t
  gap_self_id(gap_State* state);

  /// Get current user remaining invitations.
  int
  gap_self_remaining_invitations(gap_State* state);

  uint32_t*
  gap_self_favorites(gap_State* state);

  /// Publish avatar to meta.
  gap_Status
  gap_update_avatar(gap_State* state,
                    void const* data,
                    size_t size);

  /// - User ------------------------------------------------------------------

  /// Retrieve user fullname.
  char const*
  gap_user_fullname(gap_State* state,
                    uint32_t id);

  /// Retrieve user handle.
  char const*
  gap_user_handle(gap_State* state,
                  uint32_t id);

  char const*
  gap_user_realid(gap_State* state,
                  uint32_t id);

  /// Return the uri to the avatar.
  char const*
  gap_user_avatar_url(gap_State* state,
                      uint32_t user_id);

  /// Free the avatar url.
  void
  gap_free_user_avatar_url(char const* str);

  /// Get user icon data.
  gap_Status
  gap_avatar(gap_State* state,
             uint32_t user_id,
             void** data,
             size_t* size);

  /// Retrieve user with its email.
  uint32_t
  gap_user_by_email(gap_State* state,
                    char const* email);

  /// Retrieve user with their handle.
  uint32_t
  gap_user_by_handle(gap_State* state,
                     char const* handle);

  // The user directory
  char const*
  gap_user_directory(gap_State* state, char const** directory);

  /// Retrieve user status.
  gap_UserStatus
  gap_user_status(gap_State* state, uint32_t id);

  /// Search users.
  uint32_t*
  gap_search_users(gap_State* state, char const* text);

  /// Free the search users result.
  void
  gap_search_users_free(uint32_t* users);

  /// - Swaggers --------------------------------------------------------------

  /// Get the list of user's swaggers.
  uint32_t*
  gap_swaggers(gap_State* state);

  /// Free swagger list.
  void
  gap_swaggers_free(uint32_t* swaggers);

  /// Mark a user as favorite.
  gap_Status
  gap_favorite(gap_State* state,
               uint32_t const user_id);

  /// Unmark a user as favorite.
  gap_Status
  gap_unfavorite(gap_State* state,
                 uint32_t const user_id);

  /// Check if a user is a favorite.
  gap_Bool
  gap_is_favorite(gap_State* state,
                  uint32_t const user_id);

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
  uint32_t*
  gap_transactions(gap_State* state);

  /// Free transaction list.
  void
  gap_transactions_free(uint32_t* transactions);

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
  /// If the return value is 0, the operation failed.
  uint32_t
  gap_send_files(gap_State* state,
                 uint32_t id,
                 char const* const* files,
                 char const* message);

  /// Send files along with a message.
  /// If the return value is 0, the operation failed.
  uint32_t
  gap_send_files_by_email(gap_State* state,
                          char const* recipient_id,
                          char const* const* files,
                          char const* message);

  /// Cancel transaction.
  /// If the return value is 0, the operation failed.
  uint32_t
  gap_cancel_transaction(gap_State* state,
                         uint32_t id);

  /// Reject transaction.
  /// This function can only be used by the recipient of the transaction, if
  /// not already rejected or accepted.
  /// If the return value is 0, the operation failed.
  uint32_t
  gap_reject_transaction(gap_State* state,
                         uint32_t id);

  /// Accept a transaction.
  /// This function can only be used by the recipient of the transaction, if
  /// not already accepted or rejected.
  /// If the return value is 0, the operation failed.
  uint32_t
  gap_accept_transaction(gap_State* state,
                         uint32_t id);

  /// Join a transaction.
  /// This function will block as long as the transaction is not terminated
  /// and cleaned.
  uint32_t
  gap_join_transaction(gap_State* state,
                       uint32_t id);

  // Set output directory.
  gap_Status
  gap_set_output_dir(gap_State* state,
                     char const* output_path);

  char const*
  gap_get_output_dir(gap_State* state);

  /// Send user report
  gap_Status
  gap_send_user_report(char const* _user_name,
                       char const* _message,
                       char const* _file,
                       char const* _os_description);

  /// Send existing crash log to the server
  gap_Status
  gap_send_last_crash_logs(char const* _user_name,
                           char const* _crash_report,
                           char const* _state_log,
                           char const* _os_description,
                           char const* _additional_info);

# ifdef __cplusplus
} // ! extern "C"
# endif

#endif