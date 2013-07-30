#include "gap.h"
#include "State.hh"

#include <common/common.hh>

#include <lune/Lune.hh>

#include <elle/log.hh>
#include <elle/elle.hh>
#include <elle/HttpClient.hh>
#include <elle/system/Process.hh>
#include <elle/container/list.hh>
#include <CrashReporter.hh>

#include <plasma/meta/Client.hh>

#include <boost/filesystem.hpp>
#include <boost/range/join.hpp>

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

  static char**
  _cpp_stringvector_to_c_stringlist(std::vector<std::string> const& list)
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

  static
  bool
  initialize_lune()
  {
    static bool initialized = false;
    if (!initialized)
    {
      if (lune::Lune::Initialize() == elle::Status::Error)
      {
        ELLE_ERR("Cannot initialize root components");
        return initialized;
      }
      initialized = true;
    }
    return initialized;
  }

  gap_State* gap_new()
  {
    if (!initialize_lune())
      return nullptr;

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

  /// Create a new state.
  /// Returns NULL on failure.
  gap_State* gap_configurable_new(char const* meta_host,
                                  unsigned short meta_port,
                                  char const* trophonius_host,
                                  unsigned short trophonius_port,
                                  char const* apertus_host,
                                  unsigned short apertus_port)
  {
    if (!initialize_lune())
      return nullptr;

    try
    {
      return __TO_C(new surface::gap::State(meta_host,
                                            meta_port,
                                            trophonius_host,
                                            trophonius_port,
                                            apertus_host,
                                            apertus_port));
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

  gap_Bool
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
      WRAP_CPP(state, device);//, true);
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
    // WRAP_CPP(state, update_device, name);
    return gap_ok;
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
        std::vector<std::string> result;
        for (auto const& pair : users)
          result.push_back(pair.first);
        return _cpp_stringvector_to_c_stringlist(result);
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
        std::vector<std::string> result;
        for (auto const& id : swaggers)
          result.push_back(id);
        return _cpp_stringvector_to_c_stringlist(result);
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
  static inline
  void
  gap_file_users_free(char** users)
  {
    ::free(users);
  }

  // - Trophonius -----------------------------------------------------------

  gap_Status
  gap_new_swagger_callback(gap_State* state,
                           gap_new_swagger_callback_t cb)
  {
    using namespace plasma::trophonius;
    auto cpp_cb = [cb] (NewSwaggerNotification const& notif) {
      cb(notif.user_id.c_str());
    };

    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->notification_manager().new_swagger_callback(cpp_cb);
    }
    CATCH_ALL(new_swagger_callback);

    return ret;
  }

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
        cb(notif.id.c_str(), is_new);
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
#define DEFINE_TRANSACTION_GETTER(_type_, _field_, _transform_)               \
  _type_                                                                      \
  gap_transaction_ ## _field_(gap_State* state,                               \
                              char const* _id)                                \
  {                                                                           \
    assert(_id != nullptr);                                                   \
    gap_Status ret = gap_ok;                                                  \
    try                                                                       \
    { \
      return _transform_( \
        __TO_CPP(state)->transaction_manager().one(_id)._field_); \
    } \
    CATCH_ALL(transaction_ ## _field_); \
 \
    (void) ret; \
    return (_type_) 0; \
  } \
  /**/

#define NO_TRANSFORM
#define GET_CSTR(_expr_) (_expr_).c_str()

#define DEFINE_TRANSACTION_GETTER_STR(_field_)                                \
  DEFINE_TRANSACTION_GETTER(char const*, _field_, GET_CSTR)                   \
/**/
#define DEFINE_TRANSACTION_GETTER_INT(_field_)                                \
  DEFINE_TRANSACTION_GETTER(int, _field_, NO_TRANSFORM)                       \
/**/
#define DEFINE_TRANSACTION_GETTER_DOUBLE(_field_)                             \
  DEFINE_TRANSACTION_GETTER(double, _field_, NO_TRANSFORM)                    \
/**/
#define DEFINE_TRANSACTION_GETTER_BOOL(_field_)                               \
  DEFINE_TRANSACTION_GETTER(gap_Bool, _field_, NO_TRANSFORM)                  \
/**/

  DEFINE_TRANSACTION_GETTER_STR(sender_id)
  DEFINE_TRANSACTION_GETTER_STR(sender_fullname)
  DEFINE_TRANSACTION_GETTER_STR(sender_device_id)
  DEFINE_TRANSACTION_GETTER_STR(recipient_id)
  DEFINE_TRANSACTION_GETTER_STR(recipient_fullname)
  DEFINE_TRANSACTION_GETTER_STR(recipient_device_id)
  DEFINE_TRANSACTION_GETTER_STR(network_id)
  DEFINE_TRANSACTION_GETTER_STR(first_filename)
  DEFINE_TRANSACTION_GETTER_STR(message)
  DEFINE_TRANSACTION_GETTER_INT(files_count)
  DEFINE_TRANSACTION_GETTER_INT(total_size)
  DEFINE_TRANSACTION_GETTER_DOUBLE(timestamp)
  DEFINE_TRANSACTION_GETTER_BOOL(is_directory)
  // _transform_ is a cast from plasma::TransactionStatus
  DEFINE_TRANSACTION_GETTER(gap_TransactionStatus,
                            status,
                            (gap_TransactionStatus))

  float
  gap_transaction_progress(gap_State* state,
                           char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      // XXX[antony]
      return 0.0f; //return __TO_CPP(state)->transaction_manager().progress(transaction_id);
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

        std::vector<std::string> res;

        for (auto const& transaction_pair : transactions_map)
          res.push_back(transaction_pair.first);

        ELLE_DEBUG("gap_transactions() = %s", res);
        return _cpp_stringvector_to_c_stringlist(res);
      }
    CATCH_ALL(transactions);

    (void) ret;
    return nullptr;
  }

  void gap_transactions_free(char** transactions)
  {
    ::free(transactions);
  }

  gap_Status
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

      __TO_CPP(state)->send_files(recipient_id, std::move(s));
      return gap_ok;
    }
    CATCH_ALL(send_files);
    return ret;
  }

  gap_Status
  gap_cancel_transaction(gap_State* state,
                         char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->cancel_transaction(transaction_id);
    }
    CATCH_ALL(cancel_transaction);
    return ret;
  }

  gap_Status
  gap_reject_transaction(gap_State* state,
                         char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->reject_transaction(transaction_id);
    }
    CATCH_ALL(reject_transaction);
    return ret;
  }

  gap_Status
  gap_accept_transaction(gap_State* state,
                         char const* transaction_id)
  {
    assert(state != nullptr);
    assert(transaction_id != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->accept_transaction(transaction_id);
    }
    CATCH_ALL(accept_transaction);
    return ret;
  }

  gap_Status
  gap_set_output_dir(gap_State* state,
                     char const* output_path)
  {
    assert(state != nullptr);
    assert(output_path != nullptr);
    gap_Status ret = gap_ok;
    try
    {
      __TO_CPP(state)->output_dir(output_path);
    }
    CATCH_ALL(set_output_dir);
    return ret;
  }

  char const*
  gap_get_output_dir(gap_State* state)
  {
    assert(state != nullptr);

    gap_Status ret = gap_ok;
    try
    {
      auto const& directory = __TO_CPP(state)->output_dir();
      return directory.c_str();
    }
    CATCH_ALL(get_output_directory);

    (void) ret;
    return nullptr;
  }

  static
  std::string
  read_file(std::string const& filename)
  {
    std::stringstream file_content;

    file_content <<  ">>> " << filename << std::endl;

    std::ifstream f(filename);
    std::string line;
    while (f.good() && !std::getline(f, line).eof())
      file_content << line << std::endl;
    file_content << "<<< " << filename << std::endl;
    return file_content.str();
  }

  void
  gap_send_file_crash_report(char const* module,
                             char const* filename)
  {
    try
    {
      std::string file_content;
      if (filename != nullptr)
        file_content = read_file(filename);
      else
        file_content = "<<< No file was specified!";

      elle::crash::report(common::meta::host(),
                          common::meta::port(),
                          (module != nullptr ? module : "(nil)"),
                          "Crash",
                          elle::Backtrace::current(),
                          file_content);
    }
    catch (...)
    {
      ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
    }
  }

  gap_Status
  gap_send_last_crash_logs(char const* _crash_report, char const* _state_log)
  {
    try
    {
      std::string const infinit_dir = common::infinit::home();
      std::string const crash_report{_crash_report,
                                     _crash_report + strlen(_crash_report)};
      std::string const state_log{_state_log,
                                  _state_log + strlen(_state_log)};
      std::string const finder_log{"finder-plugin.log"};
      std::string const crash_report_path = infinit_dir + "/" + crash_report;
      std::string const state_log_path = infinit_dir + "/" + state_log;
      std::string const finder_log_path = "/tmp/" + finder_log;

      std::string const crash_archive = "/tmp/infinit-crash-archive";

      std::list<std::string> args{"cjf", crash_archive};
      if (boost::filesystem::exists(state_log_path))
      {
        args.push_back("-C");
        args.push_back(infinit_dir);
        args.push_back(state_log);
      }
      if (boost::filesystem::exists(crash_report_path))
      {
        args.push_back("-C");
        args.push_back(infinit_dir);
        args.push_back(crash_report);
      }
      if (boost::filesystem::exists(finder_log_path))
      {
        args.push_back("-C");
        args.push_back("/tmp");
        args.push_back(finder_log);
      }

      if (args.size() > 3)
      {
        elle::system::Process tar{"tar", args};
        tar.wait();
#if defined(INFINIT_LINUX)
        std::string b64 = elle::system::check_output("base64",
                                                     "-w0",
                                                     crash_archive);
#else
        std::string b64 = elle::system::check_output("base64", crash_archive);
#endif
        auto title = elle::sprintf("Application Crash Report");
        elle::crash::report(common::meta::host(),
                            common::meta::port(),
                            "Logs", title,
                            elle::Backtrace::current(),
                            "Logs attached",
                            b64);
      }
      else
      {
        ELLE_WARN("no logs to send");
      }
    }
    catch (...)
    {
      ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
      return gap_api_error;
    }
    return gap_ok;
  }

  gap_Status
  gap_gather_crash_reports(char const* _user_id,
                           char const* _network_id)
  {
    try
    {
      namespace fs = boost::filesystem;
      std::string const user_id{_user_id,
                                _user_id + strlen(_user_id)};
      std::string const network_id{_network_id,
                                   _network_id + strlen(_network_id)};

      std::string const user_dir = common::infinit::user_directory(user_id);
      std::string const network_dir =
        common::infinit::network_directory(user_id, network_id);

      fs::directory_iterator ndir;
      fs::directory_iterator udir;
      try
      {
        udir = fs::directory_iterator{user_dir};
        ndir = fs::directory_iterator{network_dir};
      }
      catch (fs::filesystem_error const& e)
      {
        return gap_Status::gap_file_not_found;
      }

      boost::iterator_range<fs::directory_iterator> user_range{
        udir, fs::directory_iterator{}};
      boost::iterator_range<fs::directory_iterator> network_range{
        ndir, fs::directory_iterator{}};

      std::vector<fs::path> logs;
      for (auto const& dir_ent: boost::join(user_range, network_range))
      {
        auto const& path = dir_ent.path();

        if (path.extension() == ".log")
          logs.push_back(path);

      }
      std::string filename = elle::sprintf("/tmp/infinit-%s-%s",
                                           user_id, network_id);
      std::list<std::string> args{"cjf", filename};
      for (auto const& log: logs)
        args.push_back(log.string());

      elle::system::Process tar{"tar", args};
      tar.wait();
#if defined(INFINIT_LINUX)
      std::string b64 = elle::system::check_output("base64", "-w0", filename);
#else
      std::string b64 = elle::system::check_output("base64", filename);
#endif

      auto title = elle::sprintf("Crash: Logs file for user: %s, network: %s",
                                 user_id, network_id);
      elle::crash::report(common::meta::host(),
                          common::meta::port(),
                          "Logs", title,
                          elle::Backtrace::current(),
                          "Logs attached",
                          b64);
    }
    catch (...)
    {
      ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
      return gap_error;
    }
    return gap_ok;
  }

  // Generated file.
  #include <surface/gap/gen_metrics.hh>

} // ! extern "C"
