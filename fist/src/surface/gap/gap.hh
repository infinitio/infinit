#ifndef SURFACE_GAP_GAP_HH
# define SURFACE_GAP_GAP_HH

# include <string>
# include <unordered_map>
# include <vector>

# include <surface/gap/enums.hh>
# include <surface/gap/LinkTransaction.hh>
# include <surface/gap/PeerTransaction.hh>
# include <surface/gap/User.hh>

/// gap_State is an opaque structure used in every calls.
struct gap_State;
typedef struct gap_State gap_State;

// - gap ctor & dtor --------------------------------------------------------

/// Create a new state.
/// Returns NULL on failure.
gap_State*
gap_new(bool production,
        std::string const& download_dir = "",
        std::string const& persistent_config_dir = "",
        std::string const& non_persistent_config_dir = "",
        std::string const& temp_storage_dir = "",
        bool enable_mirroring = true);

/// Release a state.
void
gap_free(gap_State* state);

/// Callback to be executed if an exception escape from the scheduler.
gap_Status
gap_critical_callback(gap_State* state, std::function<void ()> const& callback);


/// The 'error' value of ids.
uint32_t
gap_null();

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

//- Authentication & registration -------------------------------------------
void
gap_clean_state(gap_State* state);

/// Login to meta.
gap_Status
gap_login(gap_State* state,
          std::string const& email,
          std::string const& hash_password);

/// Fetch features.
std::unordered_map<std::string, std::string>
gap_fetch_features(gap_State* state);

/// Check is user is already logged.
bool
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
             std::string const& password);

//- Swaggers ----------------------------------------------------------------

gap_Status
gap_new_swagger_callback(
  gap_State* state,
  std::function<void (surface::gap::User const&)> const& callback);

gap_Status
gap_deleted_swagger_callback(
  gap_State* state,
  std::function<void (uint32_t id)> const& callback);

gap_Status
gap_deleted_favorite_callback(
  gap_State* state,
  std::function<void (uint32_t id)> const& callback);

gap_Status
gap_user_status_callback(
  gap_State* state,
  std::function<void (uint32_t id, bool status)> const& callback);

gap_Status
gap_avatar_available_callback(
  gap_State* state,
  std::function<void (uint32_t id)> const& callback);

gap_Status
gap_connection_callback(
  gap_State* state,
  std::function<void (bool status,
                      bool still_retrying,
                      std::string const& last_error)> const& callback);

/// Peer transaction callback.
gap_Status
gap_peer_transaction_callback(
  gap_State* state,
  std::function<void (surface::gap::PeerTransaction const&)> const& callback);

/// Link updated callback.
gap_Status
gap_link_callback(
  gap_State* state,
  std::function<void (surface::gap::LinkTransaction const&)> const& callback);

/// Transaction getters.
surface::gap::PeerTransaction
gap_peer_transaction_by_id(gap_State* state, uint32_t id);

float
gap_transaction_progress(gap_State* state, uint32_t id);

bool
gap_transaction_is_final(gap_State* state, uint32_t id);

bool
gap_transaction_concern_device(gap_State* state,
                               uint32_t const transaction_id,
                               bool true_if_empty_recipient = true);

/// Poll
gap_Status
gap_poll(gap_State* state);

/// - Device ----------------------------------------------------------------

/// Returns the local device status.
gap_Status
gap_device_status(gap_State* state);

/// Update the local device name.
gap_Status
gap_set_device_name(gap_State* state, std::string const& name);

/// - Self ------------------------------------------------------------------
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

std::vector<uint32_t>
gap_self_favorites(gap_State* state);

/// Publish avatar to meta.
gap_Status
gap_update_avatar(gap_State* state, void const* data, size_t size);

/// - User ------------------------------------------------------------------

std::string
gap_self_device_id(gap_State* state);

/// Get user icon data.
gap_Status
gap_avatar(gap_State* state, uint32_t id, void** data, size_t* size);

/// Refresh user's icon.
void
gap_refresh_avatar(gap_State* state, uint32_t id);

surface::gap::User
gap_user_by_id(gap_State* state, uint32_t id);

/// Retrieve user with its email.
surface::gap::User
gap_user_by_email(gap_State* state, std::string const& email);

/// Retrieve user with their handle.
surface::gap::User
gap_user_by_handle(gap_State* state, std::string const& handle);

/// Retrieve user status.
gap_UserStatus
gap_user_status(gap_State* state, uint32_t id);

/// Search users.
std::vector<surface::gap::User>
gap_users_search(gap_State* state, std::string const& text);

std::unordered_map<std::string, surface::gap::User>
gap_users_by_emails(gap_State* state, std::vector<std::string> emails);

/// - Swaggers --------------------------------------------------------------

/// Get the list of user's swaggers.
std::vector<surface::gap::User>
gap_swaggers(gap_State* state);

/// Mark a user as favorite.
gap_Status
gap_favorite(gap_State* state, uint32_t id);

/// Unmark a user as favorite.
gap_Status
gap_unfavorite(gap_State* state, uint32_t id);

/// Check if a user is a favorite.
bool
gap_is_favorite(gap_State* state, uint32_t id);

/// - Permissions ---------------------------------------------------------

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
gap_link_transaction_by_id(gap_State* state, uint32_t id);

/// Fetch list of link transactions.
std::vector<surface::gap::LinkTransaction>
gap_link_transactions(gap_State* state);

/// Get the list of transaction ids involving the user.
std::vector<surface::gap::PeerTransaction>
gap_peer_transactions(gap_State* state);

/// C++ version of gap_send_files.
/// If the return value is 0, the operation failed.
uint32_t
gap_send_files(gap_State* state,
               uint32_t id,
               std::vector<std::string> const& files,
               std::string const& message);

/// C++ version for send_files_by_email.
/// If the return value is 0, the operation failed.
uint32_t
gap_send_files_by_email(gap_State* state,
                        std::string const& email,
                        std::vector<std::string> const& files,
                        std::string const& message);

/// Cancel transaction.
/// If the return value is 0, the operation failed.
uint32_t
gap_cancel_transaction(gap_State* state, uint32_t id);

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
gap_reject_transaction(gap_State* state, uint32_t id);

/// Accept a transaction.
/// This function can only be used by the recipient of the transaction, if
/// not already accepted or rejected.
/// If the return value is 0, the operation failed.
uint32_t
gap_accept_transaction(gap_State* state, uint32_t id);

/// Return the id of an onboarding received transaction.
uint32_t
gap_onboarding_receive_transaction(gap_State* state,
                                   std::string const& file_path,
                                   uint32_t transfer_time_sec);

/// Change the peer connection status.
gap_Status
gap_onboarding_set_peer_status(gap_State* state,
                               uint32_t id,
                               bool status);

/// Change the peer availability status.
gap_Status
gap_onboarding_set_peer_availability(gap_State* state,
                                     uint32_t id,
                                     bool status);

/// Force transfer deconnection.
gap_Status
gap_onboarding_interrupt_transfer(gap_State* state, uint32_t id);

// Set output directory.
gap_Status
gap_set_output_dir(gap_State* state,
                   std::string const& output_path,
                   bool fallback);

std::string
gap_get_output_dir(gap_State* state);


typedef std::unordered_map<std::string, std::string> Additionals;
/// Metrics.
gap_Status
gap_send_metric(gap_State* state,
                UIMetricsType metric,
                Additionals additional = Additionals{});

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
