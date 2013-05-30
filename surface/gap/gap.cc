#include "gap.h"
#include "State.hh"
#include "OperationManager.hh"

#include <common/common.hh>

#include <lune/Lune.hh>

#include <elle/log.hh>
#include <elle/elle.hh>
#include <elle/HttpClient.hh>
#include <CrashReporter.hh>

#include <plasma/meta/Client.hh>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string.h>
#include <unordered_set>

ELLE_LOG_COMPONENT("infinit.surface.gap");

extern "C"
{

  /// - Utils -----------------------------------------------------------------

#define __TO_C(st)    reinterpret_cast<gap_State*>(st)
#define __TO_CPP(st)  reinterpret_cast<surface::gap::State*>(st)

# define CATCH_ALL(_func_)                                                     \
  catch (elle::HTTPException const& err)                                       \
    {                                                                          \
      ELLE_ERR(#_func_ " error: %s", err.what());                              \
      if (err.code == elle::ResponseCode::error)                               \
        ret = gap_network_error;                                               \
      else if (err.code == elle::ResponseCode::internal_server_error)          \
        ret = gap_api_error;                                                   \
      else                                                                     \
        ret = gap_internal_error;                                              \
    }                                                                          \
  catch (plasma::meta::Exception const& err)                                   \
    {                                                                          \
      ELLE_ERR(#_func_ " error: %s", err.what());                              \
      ret = (gap_Status) err.err;                                              \
    }                                                                          \
  catch (surface::gap::Exception const& err)                                   \
    {                                                                          \
      ELLE_ERR(#_func_ " error: %s", err.what());                              \
      ret = err.code;                                                          \
    }                                                                          \
  catch (elle::Exception const& err)                                           \
    {                                                                          \
      ELLE_ERR(#_func_ " error: %s", err.what());                              \
      ret = gap_internal_error;                                                \
    }                                                                          \
  catch (std::exception const& err)                                            \
    {                                                                          \
      ELLE_ERR(#_func_ " error: %s", err.what());                              \
      ret = gap_internal_error;                                                \
    }                                                                          \
  catch (...)                                                                  \
    {                                                                          \
      /*auto const &e = std::current_exception();*/                                \
      ELLE_ERR(#_func_ " unknown error type");                                 \
      ret = gap_internal_error;                                                \
      /*std::rethrow_exception(e);*/                                               \
    }                                                                          \
  /**/

// automate cpp wrapping
# define WRAP_CPP_RET(_state_, _func_, ...)                                  \
  assert(_state_ != nullptr);                                                  \
  gap_Status ret;                                                              \
  try                                                                          \
    { __TO_CPP(_state_)->_func_(__VA_ARGS__); ret = gap_ok; }                  \
  CATCH_ALL(_func_)                                                            \
  /**/

// automate cpp wrapping
# define WRAP_CPP_MANAGER_RET(_state_, _manager_, _func_, ...)                 \
  assert(_state_ != nullptr);                                                  \
  gap_Status ret;                                                              \
  try                                                                          \
  { __TO_CPP(_state_)->_manager_()._func_(__VA_ARGS__); ret = gap_ok; }        \
  CATCH_ALL(_func_)                                                            \
  /**/

# define WRAP_CPP(...)                                                         \
  WRAP_CPP_RET(__VA_ARGS__);                                                   \
  return ret                                                                   \
  /**/

# define WRAP_CPP_MANAGER(...)                                                 \
  WRAP_CPP_MANAGER_RET(__VA_ARGS__);                                           \
  return ret                                                                   \
    /**/


  static char**
  _cpp_stringlist_to_c_stringlist(std::list<std::string> const& list)
  {
    size_t total_size = (1 + list.size()) * sizeof(void*);
    for (auto const& str : list)
      total_size += str.size() + 1;

    char** ptr = reinterpret_cast<char**>(malloc(total_size));
    if (ptr == nullptr)
      return nullptr;


    char** array = ptr;
    char* cstr = reinterpret_cast<char*>(ptr + (list.size() + 1));
    for (auto const& str : list)
      {
        *array = cstr;
        ::strncpy(cstr, str.c_str(), str.size() + 1);
        ++array;
        cstr += str.size() + 1;
      }
    *array = nullptr;
    return ptr;
  }

  /// - gap ctor & dtor -----------------------------------------------------

  gap_State* gap_new()
  {
    static bool initialized = false;
    if (!initialized)
      {
        initialized = true;
        if (lune::Lune::Initialize() == elle::Status::Error)
          {
            ELLE_ERR("Cannot initialize root components");
            return nullptr;
          }
      }

    try
      {
        return __TO_C(new surface::gap::State());
      }
    catch (std::exception const& err)
      {
        ELLE_ERR("Cannot initialize gap state: %s", err.what());
        return nullptr;
      }
    catch (...)
      {
        ELLE_ERR("Cannot initialize gap state");
        return nullptr;
      }
  }

  void gap_free(gap_State* state)
  {
    delete __TO_CPP(state);
  }

  void gap_enable_debug(gap_State* state)
  {
    assert(state != nullptr);
    // FIXME
    //__TO_CPP(state)->log.level(elle::log::Logger::Level::debug);
  }

  gap_Status gap_meta_status(gap_State*)
  {
    return gap_ok;
  }

  char const*
  gap_meta_url(gap_State*)
  {
    static std::string meta_url;
    meta_url = common::meta::url(); // force retreival
    return meta_url.c_str();
  }

  gap_Status
  gap_debug(gap_State* state)
  {
    (void) state;
    return gap_ok;
  }


  gap_Status
  gap_invite_user(gap_State* state,
                  char const* email)
  {
    assert(state != nullptr);
    assert(email != nullptr);

    WRAP_CPP_MANAGER(state, user_manager, invite, email);
  }

  gap_Status
  gap_message(gap_State* state,
              const char* recipient_id,
              const char* message)
  {
    assert(recipient_id != nullptr);
    assert(message != nullptr);
    WRAP_CPP_MANAGER(state, user_manager, send_message, recipient_id, message);
  }

  //- Operation interface -------------------------------------------------------

  gap_OperationStatus gap_operation_status_failure =
    (gap_OperationStatus) surface::gap::OperationManager::OperationStatus::failure;

  gap_OperationStatus gap_operation_status_success =
    (gap_OperationStatus) surface::gap::OperationManager::OperationStatus::success;

  gap_OperationStatus gap_operation_status_running =
    (gap_OperationStatus) surface::gap::OperationManager::OperationStatus::running;

  gap_OperationStatus
  gap_operation_status(gap_State* state,
                     gap_OperationId const pid)
  {
    assert(state != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      return (gap_OperationStatus) __TO_CPP(state)->transaction_manager().status(pid);
    }
    CATCH_ALL(operation_status);
    return ret;
  }

  /// Try to finalize a operation. Returns an error if the operation
  /// does not exist, or if it's still running.
  gap_Status
  gap_operation_finalize(gap_State* state,
                       gap_OperationId const pid)
  {
    WRAP_CPP_MANAGER(state, transaction_manager, finalize, pid);
  }

  //- Authentication ----------------------------------------------------------

  char* gap_hash_password(gap_State* state,
                          char const* email,
                          char const* password)
  {
    assert(state != nullptr);
    assert(email != nullptr);
    assert(password != nullptr);

    try
      {
        std::string h = __TO_CPP(state)->hash_password(email, password);
        return ::strdup(h.c_str());
      }
    catch (std::exception const& err)
      {
        ELLE_ERR("Couldn't hash the password: %s", err.what());
      }
    return nullptr;
  }

  void gap_hash_free(char* h)
  {
    ::free(h);
  }

  gap_Status gap_login(gap_State* state,
                       char const* email,
                       char const* password)
  {
    assert(email != nullptr);
    assert(password != nullptr);

    if (gap_logged_in(state) == 1)
      return gap_ok; // Already logged in.

    WRAP_CPP(state, login, email, password);
  }

  int
  gap_logged_in(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret;
    try
      {
        int logged = __TO_CPP(state)->logged_in();
        return logged;
      }
    CATCH_ALL(logged);

    (void) ret;
    return 0;
  }

  gap_Status gap_logout(gap_State* state)
  {
    WRAP_CPP(state, logout);
  }

  gap_Status
  gap_token(gap_State* state,
            char** usertoken)
  {
    gap_Status ret = gap_error;
    try
    {
      std::string token = __TO_CPP(state)->token();
      char* new_token = strdup(token.c_str());
      if (new_token != nullptr)
      {
        ret = gap_ok;
        *usertoken = new_token;
        return ret;
      }
    }
    CATCH_ALL(token)
    return ret;
  }

  gap_Status
  gap_generation_key(gap_State* state,
                     char** usertoken)
  {
    gap_Status ret = gap_error;
    try
    {
      std::string token = __TO_CPP(state)->token_generation_key();
      char* new_token = strdup(token.c_str());
      if (new_token != nullptr)
      {
        ret = gap_ok;
        *usertoken = new_token;
        return ret;
      }
    }
    CATCH_ALL(token)
    return ret;
  }

  gap_Status gap_register(gap_State* state,
                          char const* fullname,
                          char const* email,
                          char const* password,
                          char const* device_name,
                          char const* activation_code)
  {
    assert(fullname != nullptr);
    assert(email != nullptr);
    assert(password != nullptr);
    assert(activation_code != nullptr);

    WRAP_CPP_RET(state, register_, fullname, email,
                   password, activation_code);

    if (ret == gap_ok && device_name != nullptr)
    {
      WRAP_CPP(state, update_device, device_name, true);
    }
    return ret;
  }

  gap_Status
  gap_poll(gap_State* state)
  {
    assert(state != nullptr);

    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->notification_manager().poll();
    }
    CATCH_ALL(poll);

    return ret;
  }
  /// - Device --------------------------------------------------------------
  gap_Status gap_device_status(gap_State* state)
  {
    try
    {
      if (__TO_CPP(state)->has_device())
        return gap_ok;
      else
        return gap_no_device_error;
    }
    catch (surface::gap::Exception const& err)
    {
      ELLE_ERR("Couldn't check the device: %s", err.what());
      return err.code;
    }
    catch (std::exception const& err)
    {
      ELLE_ERR("Couldn't check the device: %s", err.what());
    }
    return gap_internal_error;
  }

  gap_Status gap_set_device_name(gap_State* state,
                                 char const* name)
  {
    assert(name != nullptr);
    WRAP_CPP(state, update_device, name);
  }

  /// - Network -------------------------------------------------------------

  char** gap_networks(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret = gap_ok;
    try
      {
        auto const& networks_ids = __TO_CPP(state)->network_manager().all_ids();

        std::list<std::string> res;

        for (auto const& id : networks_ids)
          res.push_back(id);

        return _cpp_stringlist_to_c_stringlist(res);
      }
    CATCH_ALL(networks);

    (void) ret;
    return nullptr;
  }

  void gap_networks_free(char** networks)
  {
    ::free(networks);
  }

  char const* gap_network_name(gap_State* state, char const* id)
  {
    assert(state != nullptr);
    assert(id != nullptr);
    gap_Status ret;
    try
      {
        auto const& network = __TO_CPP(state)->network_manager().one(id);
        return network.name.c_str();
      }
    CATCH_ALL(network_name);

    (void) ret;
    return nullptr;
  }

  char** gap_network_users(gap_State* state, char const* id)
  {
    assert(state != nullptr);
    assert(id != nullptr);
    gap_Status ret;
    try
      {
        auto const& network = __TO_CPP(state)->network_manager().one(id);
        return _cpp_stringlist_to_c_stringlist(network.users);
      }
    CATCH_ALL(network_users);

    (void) ret;
    return nullptr;
  }

  void gap_network_users_free(char** users)
  {
    ::free(users);
  }

  char const*
  gap_create_network(gap_State* state,
                     char const* name)
  {
    assert(name != nullptr);
    gap_Status ret;
    try
    {
      auto network_id = __TO_CPP(state)->network_manager().create(name);
      return network_id.c_str();
    }
    CATCH_ALL(create_network);
    (void) ret;
    return nullptr;
  }

  gap_Status
  gap_prepare_network(gap_State* state,
                     char const* id)
  {
    assert(id != nullptr);
    gap_Status ret;
    try
    {
      __TO_CPP(state)->network_manager().prepare(id);
      ret = gap_ok;
    }
    CATCH_ALL(prepare_network);
    return ret;
  }

  gap_Status
  gap_network_add_user(gap_State* state,
                       char const* network_id,
                       char const* user_id)
  {
    assert(network_id != nullptr);
    assert(user_id != nullptr);
    gap_Status ret;
    try
    {
      auto const& user = __TO_CPP(state)->user_manager().one(user_id);
      std::string me = __TO_CPP(state)->me().id;
      __TO_CPP(state)->network_manager().add_user(network_id,
                                                  me,
                                                  user.id,
                                                  user.public_key);
      ret = gap_ok;
    }
    CATCH_ALL(network_add_user);
    return ret;
  }


  /// - Self ----------------------------------------------------------------
  char const*
  gap_user_token(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret;
    try
      {
        auto token = __TO_CPP(state)->token();
        return token.c_str();
      }
    CATCH_ALL(user_token);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_self_email(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret;
    try
      {
        auto email = __TO_CPP(state)->me().email;
        return email.c_str();
      }
    CATCH_ALL(user_email);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_self_id(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret;
    try
      {
        auto user_id = __TO_CPP(state)->me().id;
        return user_id.c_str();
      }
    CATCH_ALL(user_token);

    (void) ret;
    return nullptr;
  }

  /// Get current user remaining invitations.
  int
  gap_self_remaining_invitations(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret;
    try
      {
        return __TO_CPP(state)->me().remaining_invitations;
      }
    CATCH_ALL(user_remaining_invitations);

    (void) ret;
    return 0;
  }

  /// - User ----------------------------------------------------------------

  char const*
  gap_user_directory(gap_State* state, char const** directory)
  {
    gap_Status ret;
    try
    {
      std::string path = __TO_CPP(state)->user_directory();
      char const* tmp = strdup(path.c_str());
      if (directory != nullptr)
      {
        *directory = tmp;
      }
      return tmp;
    }
    CATCH_ALL(user_directory);
    (void)ret; // this CATCH_ALL Macro sucks.
    return nullptr;
  }

  char const* gap_user_fullname(gap_State* state, char const* id)
  {
    assert(state != nullptr);
    assert(id != nullptr);
    gap_Status ret;
    try
      {
        auto const& user = __TO_CPP(state)->user_manager().one(id);
        return user.fullname.c_str();
      }
    CATCH_ALL(user_fullname);

    (void) ret;
    return nullptr;
  }

  char const* gap_user_handle(gap_State* state, char const* id)
  {
    assert(state != nullptr);
    assert(id != nullptr);
    gap_Status ret;
    try
      {
        auto const& user = __TO_CPP(state)->user_manager().one(id);
        return user.handle.c_str();
      }
    CATCH_ALL(user_email);

    (void) ret;
    return nullptr;
  }

  gap_Status
  gap_user_icon(gap_State* state,
                char const* user_id,
                void** data,
                size_t* size)
  {
    assert(state != nullptr);
    assert(user_id != nullptr);
    *data = nullptr;
    *size = 0;
    gap_Status ret = gap_ok;
    try
      {
        auto pair = __TO_CPP(state)->user_manager().icon(user_id).release();
        *data = pair.first.release();
        *size = pair.second;
      }
    CATCH_ALL(user_icon);
    return ret;
  }

  void gap_user_icon_free(void* data)
  {
    free(data);
  }

  char const* gap_user_by_email(gap_State* state, char const* email)
  {
    assert(state != nullptr);
    assert(email != nullptr);
    gap_Status ret;
    try
      {
        auto const& user = __TO_CPP(state)->user_manager().one(email);
        return user.id.c_str();
      }
    CATCH_ALL(user_by_email);

    (void) ret;
    return nullptr;
  }

  char** gap_search_users(gap_State* state, char const* text)
  {
    assert(state != nullptr);

    gap_Status ret;
    try
      {
        auto users = __TO_CPP(state)->user_manager().search(text);
        std::list<std::string> result;
        for (auto const& pair : users)
          result.push_back(pair.first);
        return _cpp_stringlist_to_c_stringlist(result);
      }
    CATCH_ALL(search_users);

    (void) ret;
    return nullptr;
  }

  void gap_search_users_free(char** users)
  {
    ::free(users);
  }

  gap_UserStatus
  gap_user_status(gap_State* state, char const* user_id)
  {
    gap_Status ret = gap_ok;
    try
    {
      return (gap_UserStatus) __TO_CPP(state)->user_manager().one(user_id).status;
    }
    CATCH_ALL(user_status);

    return (gap_UserStatus) ret;
  }

  char**
  gap_swaggers(gap_State* state)
  {
    assert(state != nullptr);

    gap_Status ret;
    try
      {
        auto swaggers = __TO_CPP(state)->user_manager().swaggers();
        std::list<std::string> result;
        for (auto const& pair : swaggers)
          result.push_back(pair.first);
        return _cpp_stringlist_to_c_stringlist(result);
      }
    CATCH_ALL(get_swaggers);

    (void) ret;
    return nullptr;
  }

  void
  gap_swaggers_free(char** swaggers)
  {
    ::free(swaggers);
  }

  /// - Permissions ---------------------------------------------------------
  void gap_file_users_free(char** users)
  {
    ::free(users);
  }

  // - Trophonius -----------------------------------------------------------

  gap_Status
  gap_user_status_callback(gap_State* state,
                           gap_user_status_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (UserStatusNotification const& notif) {
        cb(notif.user_id.c_str(), (gap_UserStatus) notif.status);
    };

    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->notification_manager().user_status_callback(cpp_cb);
    }
    CATCH_ALL(user_status_callback);

    return ret;
  }

  gap_Status
  gap_transaction_callback(gap_State* state,
                           gap_transaction_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (TransactionNotification const& notif, bool is_new) {
        cb(notif.transaction.id.c_str(), is_new);
    };

    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->notification_manager().transaction_callback(cpp_cb);
    }
    CATCH_ALL(transaction_callback);

    return ret;
  }

  gap_Status
  gap_message_callback(gap_State* state,
                       gap_message_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (MessageNotification const& notif) {
        cb(notif.sender_id.c_str(), notif.message.c_str());
    };

    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->notification_manager().message_callback(cpp_cb);
    }
    CATCH_ALL(message_callback);

    return ret;
  }

  gap_Status
  gap_on_error_callback(gap_State* state,
                        gap_on_error_callback_t cb)
  {
    auto cpp_cb = [cb] (gap_Status s,
                        std::string const& str,
                        std::string const& tid)
    {
      cb(s, str.c_str(), tid.c_str());
    };

    WRAP_CPP_MANAGER(state, notification_manager, on_error_callback, cpp_cb);
  }

  /// Transaction getters.
  // Macroify all of this.
  char const*
  gap_transaction_sender_id(gap_State* state,
                            char const* _id)
  {
    assert(_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.sender_id.c_str();
    }
    CATCH_ALL(transaction_sender_id);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_sender_fullname(gap_State* state,
                                  char const* _id)
  {
    assert(_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.sender_fullname.c_str();
    }
    CATCH_ALL(transaction_sender_fullname);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_sender_device_id(gap_State* state,
                                   char const* _id)
  {
    assert(_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.sender_device_id.c_str();
    }
    CATCH_ALL(transaction_device_id);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_recipient_id(gap_State* state,
                               char const* _id)
  {
    assert(_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.recipient_id.c_str();
    }
    CATCH_ALL(transaction_recipient_id);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_recipient_fullname(gap_State* state,
                                     char const* _id)
  {
    assert(_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.recipient_fullname.c_str();
    }
    CATCH_ALL(transaction_recipient_fullname);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_recipient_device_id(gap_State* state,
                                      char const* _id)
  {
    assert(_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.recipient_device_id.c_str();
    }
    CATCH_ALL(transaction_recipient_device_id);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_network_id(gap_State* state,
                             char const* _id)
  {
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.network_id.c_str();
    }
    CATCH_ALL(transaction_network_id);

    (void) ret;
    return nullptr;
  }

  char const*
  gap_transaction_first_filename(gap_State* state,
                                 char const* _id)
  {
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.first_filename.c_str();
    }
    CATCH_ALL(transaction_first_filename);

    (void) ret;
    return nullptr;
  }

  int
  gap_transaction_files_count(gap_State* state,
                              char const* _id)
  {
    gap_Status ret = gap_ok;
    try
    {
      auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
      return transaction.files_count;
    }
    CATCH_ALL(transaction_files_count);

    return ret;
  }

  int
  gap_transaction_total_size(gap_State* state,
                             char const* _id)
  {
    gap_Status ret = gap_ok;
    try
      {
        auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
        return transaction.total_size;
      }
    CATCH_ALL(transaction_total_size);

    return ret;
  }

  double
  gap_transaction_timestamp(gap_State* state,
                            char const* transaction_id)
  {
    gap_Status ret = gap_ok;
    try
      {
        auto const& transaction =
          __TO_CPP(state)->transaction_manager().one(transaction_id);
        return transaction.timestamp / 1000.0;
      }
    CATCH_ALL(transaction_timestamp);
    (void) ret;
    return 0.0;
  }

  // gap_Bool
  int
  gap_transaction_is_directory(gap_State* state,
                               char const* _id)
  {
    gap_Status ret = gap_ok;
    try
      {
        auto const& transaction = __TO_CPP(state)->transaction_manager().one(_id);
        return transaction.is_directory;
      }
    CATCH_ALL(transaction_is_directory);

    return ret;
  }

  gap_TransactionStatus
  gap_transaction_status(gap_State* state,
                         char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret;

    try
      {
        auto const& transaction = __TO_CPP(state)->transaction_manager().one(transaction_id);
        return (gap_TransactionStatus) transaction.status;
      }
    CATCH_ALL(transaction_status);

    (void) ret;
    return gap_TransactionStatus::gap_transaction_status_none;
  }

  char const*
  gap_transaction_message(gap_State* state,
                          char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
      {
        auto const& transaction = __TO_CPP(state)->transaction_manager().one(transaction_id);
        return transaction.message.c_str();
      }
    CATCH_ALL(transaction_message);

    (void) ret;
    return nullptr;
  }

  float
  gap_transaction_progress(gap_State* state,
                           char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      return __TO_CPP(state)->transaction_manager().progress(transaction_id);
    }
    CATCH_ALL(transaction_progress);

    (void) ret;
    return 0;
  }

  gap_Status
  gap_transaction_sync(gap_State* state,
                         char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->transaction_manager().sync(transaction_id);
    }
    CATCH_ALL(transaction_update);
    return ret;
  }
  // - Notifications -----------------------------------------------------------

  gap_Status
  gap_pull_notifications(gap_State* state,
                         int count,
                         int offset)
  {
    WRAP_CPP_MANAGER(state, notification_manager, pull, count, offset, false);
  }

  gap_Status
  gap_pull_new_notifications(gap_State* state,
                             int count,
                             int offset)
  {
    WRAP_CPP_MANAGER(state, notification_manager, pull, count, offset, true);
  }

  gap_Status
  gap_notifications_read(gap_State* state)
  {
    WRAP_CPP_MANAGER(state, notification_manager, read);
  }

 char** gap_transactions(gap_State* state)
  {
    assert(state != nullptr);
    gap_Status ret = gap_ok;
    try
      {
        auto const& transactions_map = __TO_CPP(state)->transaction_manager().all();

        std::list<std::string> res;

        for (auto const& transaction_pair : transactions_map)
          res.push_back(transaction_pair.first);

        return _cpp_stringlist_to_c_stringlist(res);
      }
    CATCH_ALL(transactions);

    (void) ret;
    return nullptr;
  }

  void gap_transactions_free(char** transactions)
  {
    ::free(transactions);
  }

  gap_OperationStatus
  gap_send_files(gap_State* state,
                 char const* recipient_id,
                 char const* const* files)
  {
    assert(state != nullptr);
    assert(recipient_id != nullptr);
    assert(files != nullptr);

    gap_Status ret = gap_ok;

    try
    {
      std::unordered_set<std::string> s;

        while (*files != nullptr)
        {
          s.insert(*files);
          ++files;
        }

      return (gap_OperationStatus) __TO_CPP(state)->transaction_manager().send_files(recipient_id, s);
    }
    CATCH_ALL(send_files);

    return ret;
  }

  gap_Status
  gap_update_transaction(gap_State* state,
                         char const* transaction_id,
                         gap_TransactionStatus status)
  {
    assert(transaction_id != nullptr);

    if(status <= gap_TransactionStatus::gap_transaction_status_none
       || status >= gap_TransactionStatus::gap_transaction_status__count)
      return gap_error;

    WRAP_CPP_MANAGER_RET(state,
                         transaction_manager,
                         update,
                         transaction_id,
                         status);

    return ret;
  }

  gap_Status
  gap_set_output_dir(gap_State* state,
                     char const* output_path)
  {
    assert(state != nullptr);
    assert(output_path != nullptr);

    WRAP_CPP_MANAGER_RET(state,
                         transaction_manager,
                         output_dir,
                         output_path);

    return ret;
  }

  char const*
  gap_get_output_dir(gap_State* state)
  {
    assert(state != nullptr);

    gap_Status ret = gap_ok;
    try
    {
      auto const& directory = __TO_CPP(state)->transaction_manager().output_dir();
      return directory.c_str();
    }
    CATCH_ALL(get_output_directory);

    (void) ret;
    return nullptr;
  }


  void
  gap_send_file_crash_report(char const* module,
                             char const* filename)
  {
    std::string file_content = ">>>\n";
    if (filename != nullptr)
    {
      std::ifstream f(filename);
      std::string line;
      while (f.good() && !std::getline(f, line).eof())
        file_content += line + "\n";
      file_content += "<<< " + std::string{filename} + "\n";
    }
    else
      file_content = "<<< No file was specified!";


    elle::crash::report(common::meta::host(),
                        common::meta::port(),
                        (module != nullptr ? module : "(nil)"),
                        "Crash",
                        elle::Backtrace::current(),
                        file_content);
  }

} // ! extern "C"
