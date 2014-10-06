#ifndef SURFACE_GAP_GAP_HH
# define SURFACE_GAP_GAP_HH

# include <string>
# include <unordered_map>
# include <vector>

# include <surface/gap/enums.hh>
# include <surface/gap/LinkTransaction.hh>

typedef int gap_Bool;

/// gap_State is an opaque structure used in every calls.
struct gap_State;
typedef struct gap_State gap_State;

// - gap ctor & dtor --------------------------------------------------------

/// Create a new state.
/// Returns NULL on failure.
gap_State* gap_new(bool production, std::string const& download_dir);

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

/// Set connection status
gap_Status
gap_internet_connection(gap_State* state, bool connected);

/// Set proxy.
gap_Status
gap_set_proxy(gap_State* state,
              gap_ProxyType type,
              std::string const& host,
              uint16_t port,
              std::string const& username,
              std::string const& password);

/// Unset proxy.
gap_Status
gap_unset_proxy(gap_State* state, gap_ProxyType type);

/// Login to meta.
gap_Status
gap_login(gap_State* state,
          char const* email,
          char const* hash_password);

/// Fetch features.
std::unordered_map<std::string, std::string>
gap_fetch_features(gap_State* state);

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
             std::string const& fullname,
             std::string const& email,
             std::string const& hashed_password);

//- Swaggers ----------------------------------------------------------------

typedef void (*gap_new_swagger_callback_t)(uint32_t id);

gap_Status
gap_new_swagger_callback(gap_State* state,
                         gap_new_swagger_callback_t cb);

typedef void (*gap_deleted_swagger_callback_t)(uint32_t id);
gap_Status
gap_deleted_swagger_callback(gap_State* state,
                             gap_deleted_swagger_callback_t cb);

typedef void (*gap_deleted_favorite_callback_t)(uint32_t id);
gap_Status
gap_deleted_favorite_callback(gap_State* state,
                             gap_deleted_favorite_callback_t cb);

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

/// Link updated callback.
gap_Status
gap_link_callback(
  gap_State* state,
  std::function<void (surface::gap::LinkTransaction const&)> const& callback);

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

bool
gap_transaction_is_final(gap_State* state,
                         uint32_t const transaction_id);

bool
gap_transaction_concern_device(gap_State* state,
                               uint32_t const transaction_id,
                               bool true_if_empty_recipient = true);


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
std::string
gap_self_email(gap_State* state);

// Set current user email.
gap_Status
gap_set_self_email(gap_State* state,
                   std::string const& email,
                   std::string const& password);

/// Get current user fullname.
std::string
gap_self_fullname(gap_State* state);

/// Set current user fullname
gap_Status
gap_set_self_fullname(gap_State* state, std::string const& fullname);

/// Get current user handle.
std::string
gap_self_handle(gap_State* state);

gap_Status
gap_set_self_handle(gap_State* state, std::string const& handle);

gap_Status
gap_change_password(gap_State* state,
                    std::string const& old_password,
                    std::string const& new_password);

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

gap_Bool
gap_user_ghost(gap_State* state, uint32_t id);

gap_Bool
gap_user_deleted(gap_State* state, uint32_t id);

char const*
gap_user_realid(gap_State* state,
                uint32_t id);

std::string
gap_self_device_id(gap_State* state);

/// Get user icon data.
gap_Status
gap_avatar(gap_State* state,
           uint32_t user_id,
           void** data,
           size_t* size);

/// Refresh user's icon.
void
gap_refresh_avatar(gap_State* state, uint32_t user_id);

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
std::vector<uint32_t>
gap_users_search(gap_State* state, std::string const& text);

std::unordered_map<std::string, uint32_t>
gap_users_by_emails(gap_State* state, std::vector<std::string> emails);

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

/// Check if a transaction is a link transaction.
bool
gap_is_link_transaction(gap_State* state, uint32_t id);

/// Create a link transaction.
uint32_t
gap_create_link_transaction(gap_State* state,
                            std::vector<std::string> const& files,
                            std::string const& message);

/// Fetch a transaction by id.
surface::gap::LinkTransaction
gap_link_transaction_by_id(gap_State* state,
                           uint32_t id);

/// Fetch list of link transactions.
std::vector<surface::gap::LinkTransaction>
gap_link_transactions(gap_State* state);

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

/// Cpp version of gap_send_files.
/// If the return value is 0, the operation failed.
uint32_t
gap_send_files(gap_State* state,
               uint32_t id,
               std::vector<std::string> const& files,
               std::string const& message);

/// Send files along with a message.
/// If the return value is 0, the operation failed.
uint32_t
gap_send_files_by_email(gap_State* state,
                        char const* recipient_id,
                        char const* const* files,
                        char const* message);

/// Cpp version gor send_files_by_email.
/// If the return value is 0, the operation failed.
uint32_t
gap_send_files_by_email(gap_State* state,
                        std::string const& email,
                        std::vector<std::string> const& files,
                        std::string const& message);

/// Cancel transaction.
/// If the return value is 0, the operation failed.
uint32_t
gap_cancel_transaction(gap_State* state,
                       uint32_t id);

/// Delete transaction.
/// This operation can only be performed on LinkTransactions.
/// If the return value is 0, the operation failed.
uint32_t
gap_delete_transaction(gap_State* state, uint32_t id);

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

/// Return the id of an onboarding received transaction.
uint32_t
gap_onboarding_receive_transaction(gap_State* state,
                                   std::string const& file_path,
                                   uint32_t transfer_time_sec);

/// Change the peer connection status.
gap_Status
gap_onboarding_set_peer_status(gap_State* state,
                               uint32_t transaction_id,
                               bool status);

/// Change the peer availability status.
gap_Status
gap_onboarding_set_peer_availability(gap_State* state,
                                     uint32_t transaction_id,
                                     bool status);

/// Force transfer deconnection.
gap_Status
gap_onboarding_interrupt_transfer(gap_State* state,
                                  uint32_t transaction_id);

// Set output directory.
gap_Status
gap_set_output_dir(gap_State* state, std::string const& output_path);

std::string
gap_get_output_dir(gap_State* state);

/// Send user report
gap_Status
gap_send_user_report(gap_State* state,
                     std::string const& user_name,
                     std::string const& message,
                     std::string const& file);

/// Send existing crash log to the server
gap_Status
gap_send_last_crash_logs(gap_State* state,
                         std::string const& user_name,
                         std::string const& crash_report,
                         std::string const& state_log,
                         std::string const& additional_info);

#endif
