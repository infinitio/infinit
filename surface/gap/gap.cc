#include <surface/gap/gap.hh>
#include <surface/gap/gap_bridge.hh>
#include <surface/gap/State.hh>
#include <surface/gap/Transaction.hh>

#include <infinit/oracles/meta/Client.hh>

#include <common/common.hh>

#include <elle/log.hh>
#include <elle/elle.hh>
#include <elle/finally.hh>
#include <elle/HttpClient.hh>
#include <elle/container/list.hh>
#include <CrashReporter.hh>

#include <reactor/scheduler.hh>

#include <boost/range/join.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string.h>
#include <unordered_set>

ELLE_LOG_COMPONENT("infinit.surface.gap");

template <typename T>
inline static
T*
vector_to_pointer(std::vector<T> const& values)
{
  T* out = (T*) malloc(sizeof(T) * values.size() + 1);

  if (out == nullptr)
    return nullptr;

  memcpy(out, values.data(), values.size() * sizeof(T));

  out[values.size()] = 0;

  return out;
}

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

gap_State* gap_new(bool production)
{
  try
  {
    gap_State* state = new gap_State(production);
    return state;
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

uint32_t
gap_null()
{
  return surface::gap::null_id;
}

/// Create a new state.
/// Returns NULL on failure.
gap_State* gap_configurable_new(bool production,
                                char const* meta_protocol,
                                char const* meta_host,
                                unsigned short meta_port,
                                char const* trophonius_host,
                                unsigned short trophonius_port)
{
  try
  {
    gap_State* state = new gap_State(production,
                                     meta_protocol,
                                     meta_host,
                                     meta_port,
                                     trophonius_host,
                                     trophonius_port);
    return state;
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
  delete state;
  ELLE_LOG("State successfully destroyed");
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

char*
gap_hash_password(gap_State* state,
                  char const* email,
                  char const* password)
{
  assert(email != nullptr);
  assert(password != nullptr);

  return run<char*>(state,
                    "hash_password",
                    [&] (surface::gap::State& state) -> char*
                    {
                      std::string h = state.hash_password(email, password);
                      return ::strdup(h.c_str());
                    });
}

void gap_hash_free(char* h)
{
  ::free(h);
}

char const*
gap_meta_down_message(gap_State* _state)
{
  return run<char const*>(_state,
                          "meta down message",
                          [&] (surface::gap::State& state) -> char const*
                          {
                            auto meta_message = state.meta_message();
                            return meta_message.c_str();
                          });
}

gap_Status
gap_login(gap_State* _state,
          char const* email,
          char const* hash_password)
{
  assert(email != nullptr);
  assert(hash_password != nullptr);

  return run<gap_Status>(_state,
                         "login",
                         [&] (surface::gap::State& state) -> gap_Status
                         {
                           state.login(email, hash_password);
                           return gap_ok;
                         });
}

gap_Bool
gap_logged_in(gap_State* state)
{
  return run<gap_Bool>(state,
                       "logged",
                       [&] (surface::gap::State& state) -> gap_Bool
                       {
                         return state.logged_in();
                       });
}

gap_Status
gap_logout(gap_State* state)
{
  return run<gap_Status>(state,
                       "logout",
                       [&] (surface::gap::State& state) -> gap_Status
                       {
                         state.logout();
                         return gap_ok;
                       });
}

gap_Status
gap_register(gap_State* state,
             char const* fullname,
             char const* email,
             char const* password,
             char const* device_name,
             char const* activation_code)
{
  auto ret = run<gap_Status>(state,
                             "register",
                             [&] (surface::gap::State& state) -> gap_Status
                             {
                               state.register_(fullname, email, password, activation_code);
                               return gap_ok;
                             });
  return ret;
}

gap_Status
gap_poll(gap_State* state)
{
  gap_Status ret = gap_ok;
  try
  {
    state->state().poll();
  }
  catch (elle::http::Exception const& err)
  {
    ELLE_ERR("poll error: %s", err.what());
    if (err.code == elle::http::ResponseCode::error)
      ret = gap_network_error;
    else if (err.code == elle::http::ResponseCode::internal_server_error)
      ret = gap_api_error;
    else
      ret = gap_internal_error;
  }
  catch (infinit::oracles::meta::Exception const& err)
  {
    ELLE_ERR("poll error: %s", err.what());
    ret = (gap_Status) err.err;
  }
  catch (surface::gap::Exception const& err)
  {
    ELLE_ERR("poll error: %s", err.what());
    ret = err.code;
  }
  catch (...) // XXX.
  {
    ELLE_ERR("poll error %s", elle::exception_string());
    ret = gap_error;
  }
  return ret;
}

/// - Device --------------------------------------------------------------
gap_Status
gap_device_status(gap_State* state)
{
  return run<gap_Status>(state,
                        "device status",
                        [&] (surface::gap::State& state) -> gap_Status
                        {
                          if (state.has_device())
                            return gap_ok;
                          else
                            return gap_no_device_error;
                        });
}

gap_Status
gap_set_device_name(gap_State* state,
                    char const* name)
{
  assert(name != nullptr);
  return run<gap_Status>(state,
                         "set device name",
                         [&] (surface::gap::State& state) -> gap_Status
                         {
                           state.update_device(name);
                           return gap_ok;
                         });
}

/// - Self ----------------------------------------------------------------
char const*
gap_self_email(gap_State* state)
{
  return run<char const*>(state,
                          "self email",
                          [&] (surface::gap::State& state) -> char const*
                          {
                            auto email = state.me().email;
                            return email.c_str();
                          });
}

uint32_t
gap_self_id(gap_State* state)
{
  return run<uint32_t>(
    state,
    "self id",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.user_indexes().at(state.me().id);
    });
}

/// Get current user remaining invitations.
int
gap_self_remaining_invitations(gap_State* state)
{
  return run<int>(state,
                  "user invitations",
                  [&] (surface::gap::State& state) -> int
                  {
                    return state.me().remaining_invitations;
                  });
}

uint32_t*
gap_self_favorites(gap_State* state)
{
  auto ret = run<std::vector<uint32_t>>(
    state,
    "favorites",
    [&] (surface::gap::State& state) -> std::vector<uint32_t>
    {
      std::vector<uint32_t> values;
      for (std::string const& fav: state.me().favorites)
      {
        state.user(fav); // update user indexes
        values.push_back(state.user_indexes().at(fav));
      }
      return values;
    });

  if (ret.status() != gap_ok)
    return nullptr;

  return vector_to_pointer(ret.value());
}

/// Publish avatar to meta.
gap_Status
gap_update_avatar(gap_State* state,
                  void const* data,
                  size_t size)
{
  assert(data != nullptr);

  elle::Buffer picture(data, size);

  return run<gap_Status>(
    state,
    "update avatar",
    [&] (surface::gap::State& state)
    {
      state.set_avatar(picture);
      return gap_ok;
    });
}

/// - User ----------------------------------------------------------------

char const*
gap_user_directory(gap_State* state, char const** directory)
{
  return run<char const*>(state,
                          "user directory",
                          [&] (surface::gap::State& state) -> char const*
                          {
                            std::string path = state.user_directory();
                            char const* tmp = strdup(path.c_str());
                            if (directory != nullptr)
                            {
                              *directory = tmp;
                            }
                            return tmp;
                          });
}

char const* gap_user_fullname(gap_State* state,
                              uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<char const*>(state,
                          "user fullname",
                          [&] (surface::gap::State& state) -> char const*
                          {
                            auto const& user = state.user(id);
                            return user.fullname.c_str();
                          });
  return nullptr;
}

char const* gap_user_handle(gap_State* state,
                            uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<char const*>(state,
                          "user handle",
                          [&] (surface::gap::State& state) -> char const*
                          {
                            auto const& user = state.user(id);
                            return user.handle.c_str();
                          });
  return nullptr;
}

char const*
gap_user_realid(gap_State* state,
                uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<char const*>(state,
                          "user handle",
                          [&] (surface::gap::State& state) -> char const*
                          {
                            auto const& user = state.user(id);
                            return user.id.c_str();
                          });
}

char const*
gap_user_avatar_url(gap_State* state,
                    uint32_t user_id)
{
  assert(user_id != surface::gap::null_id);

  std::string res = run<std::string>(
    state,
    "user avatar url",
    [&] (surface::gap::State& state) -> std::string
    {
      return elle::sprintf(
        "%s:%s/user/%s/avatar",
        state.meta().host(),
        state.meta().port(),
        state.user(user_id).id);
    });

  auto size = res.size() + 1;
  char const* cstr = static_cast<char const*>(::malloc(sizeof(char) * size));
  ::strncpy((char *) cstr, res.c_str(), size);
  return cstr;
}

void
gap_free_user_avatar_url(char const* str)
{
  free((void*) str);
}

gap_Status
gap_avatar(gap_State* state,
           uint32_t user_id,
           void** data,
           size_t* size)
{
  assert(user_id != surface::gap::null_id);

  return run<gap_Status>(
    state,
    "user avatar url",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto res = state.user_icon(state.user(user_id).id);

      *data = (void*) res.contents();
      *size = res.size();

      return gap_ok;
    });
}

uint32_t
gap_user_by_email(gap_State* state,
                  char const* email)
{
  assert(email != nullptr);
  return run<uint32_t>(
    state,
    "user by email",
    [&] (surface::gap::State& state) -> uint32_t
    {
      auto user = state.user(email, true);
      return state.user_indexes().at(user.id);
    });
}

uint32_t
gap_user_by_handle(gap_State* state,
                   char const* handle)
{
  assert(handle != nullptr);
  return run<uint32_t>(
    state,
    "user by handle",
    [&] (surface::gap::State& state) -> uint32_t
    {
      auto user = state.user_from_handle(handle);
      return state.user_indexes().at(user.id);
    });
}

uint32_t*
gap_search_users(gap_State* state,
                 char const* text)
{
  assert(text != nullptr);
  auto ret = run<surface::gap::State::UserIndexes>(
    state,
    "users",
    [&] (surface::gap::State& state) -> surface::gap::State::UserIndexes
    {
      return state.user_search(text);
    });

  if (ret.status() != gap_ok)
    return nullptr;

  std::vector<uint32_t> values(ret.value().size());
  std::copy(std::begin(ret.value()), std::end(ret.value()), values.begin());

  return vector_to_pointer(values);
}

std::unordered_map<std::string, uint32_t>
gap_users_by_emails(gap_State* state, std::vector<std::string> emails)
{
  assert(emails.size() != 0);
  auto ret = run<std::unordered_map<std::string, uint32_t>>(
    state,
    "emails and users",
    [&] (surface::gap::State& state) -> std::unordered_map<std::string, uint32_t>
    {
      return state.users_by_emails(emails);
    });
  return ret.value();
}

void gap_search_users_free(uint32_t* users)
{
  ::free(users);
}

gap_UserStatus
gap_user_status(gap_State* state,
                uint32_t id)
{
  assert(id != surface::gap::null_id);

  return run<gap_UserStatus>(
    state,
    "user status",
    [&] (surface::gap::State& state) -> gap_UserStatus
    {
      return (gap_UserStatus) state.users().at(id).status();
    });
}

uint32_t*
gap_swaggers(gap_State* state)
{
  auto ret = run<surface::gap::State::UserIndexes>(
    state,
    "swaggers",
    [&] (surface::gap::State& state) -> surface::gap::State::UserIndexes
    {
      return state.swaggers();
    });

  if (ret.status() != gap_ok)
    return nullptr;

  std::vector<uint32_t> values(ret.value().size());
  std::copy(std::begin(ret.value()), std::end(ret.value()), values.begin());

  return vector_to_pointer(values);
}

void
gap_swaggers_free(uint32_t* swaggers)
{
  ::free(swaggers);
}

gap_Status
gap_favorite(gap_State* state,
             uint32_t const user_id)
{
  return run<gap_Status>(
    state,
    "favorite",
    [&user_id] (surface::gap::State& state ) {
      std::string id = state.users().at(user_id).id;
      state.meta().favorite(id);
      // XXX Should be notification driven
      std::list<std::string>& favorites =
        const_cast<std::list<std::string>&>(state.me().favorites);
      if (std::find(favorites.begin(),
                    favorites.end(),
                    id) == favorites.end())
        favorites.push_back(id);
      if (state.metrics_reporter())
        state.metrics_reporter()->user_favorite(id);
      return gap_ok;
    });
}

gap_Status
gap_unfavorite(gap_State* state,
               uint32_t const user_id)
{
  return run<gap_Status>(
    state,
    "favorite",
    [&user_id] (surface::gap::State& state ) {
      std::string id = state.users().at(user_id).id;
      state.meta().unfavorite(id);
      // XXX Should be notification driven
      std::list<std::string>& favorites =
        const_cast<std::list<std::string>&>(state.me().favorites);
      auto it = std::find(favorites.begin(),
                          favorites.end(),
                          id);
      if (it != favorites.end())
        favorites.erase(it);
      if (state.metrics_reporter())
        state.metrics_reporter()->user_unfavorite(id);
      return gap_ok;
    });
}

gap_Bool
gap_is_favorite(gap_State* state,
                uint32_t const user_id)
{
  return run<gap_Bool>(
    state,
    "is_favorite",
    [&user_id] (surface::gap::State& state) {
      std::string id = state.users().at(user_id).id;
      auto it = std::find(state.me().favorites.begin(),
                          state.me().favorites.end(),
                          id);
      return (it != state.me().favorites.end());
    }
  );
}

// - Trophonius -----------------------------------------------------------

gap_Status
gap_new_swagger_callback(gap_State* state,
                         gap_new_swagger_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::NewSwaggerNotification const& notif)
    {
      cb(notif.id);
    };

  return run<gap_Status>(
    state,
    "new swagger callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::NewSwaggerNotification>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_user_status_callback(gap_State* state,
                         gap_user_status_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::UserStatusNotification const& notif)
  {
    cb(notif.id, (gap_UserStatus) notif.status);
  };

  return run<gap_Status>(
    state,
    "user callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::UserStatusNotification>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_avatar_available_callback(gap_State* state,
                              gap_avatar_available_callback_t cb)
{
  auto cpp_cb = [cb] (
    surface::gap::State::AvatarAvailableNotification const& notif)
  {
    cb(notif.id);
  };

  return run<gap_Status>(
    state,
    "avatar available callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::AvatarAvailableNotification>(
        cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_connection_callback(gap_State* state,
                        gap_connection_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::ConnectionStatus const& notif)
    {
      cb((gap_UserStatus) notif.status);
    };

  return run<gap_Status>(
    state,
    "connection status callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::ConnectionStatus>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_kicked_out_callback(gap_State* state,
                        gap_kicked_out_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::KickedOut const&)
    {
      cb();
    };

  return run<gap_Status>(
    state,
    "kicked out callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::KickedOut>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_trophonius_unavailable_callback(gap_State* state,
                                    gap_trophonius_unavailable_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::TrophoniusUnavailable const&)
    {
      cb();
    };

  return run<gap_Status>(
    state,
    "trophonius unavailable callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::TrophoniusUnavailable>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_transaction_callback(gap_State* state,
                         gap_transaction_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::Transaction::Notification const& notif)
  {
    cb(notif.id, notif.status);
  };

  return run<gap_Status>(
    state,
    "transaction callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::Transaction::Notification>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_message_callback(gap_State* state,
                     gap_message_callback_t cb)
{
  // using namespace plasma::trophonius;
  // auto cpp_cb = [cb] (MessageNotification const& notif) {
  //     cb(notif.sender_id.c_str(), notif.message.c_str());
  // };

  // return run<gap_Status>(
  //   state,
  //   "message callback",
  //   [&] (surface::gap::State& state) -> gap_Status
  //   {
  //     state.notification_manager().message_callback(cpp_cb);
  //     return gap_ok;
  //   });
  return gap_ok;
}

gap_Status
gap_on_error_callback(gap_State* state,
                      gap_on_error_callback_t cb)
{
  // auto cpp_cb = [cb] (gap_Status s,
  //                     std::string const& str,
  //                     std::string const& tid)
  // {
  //   cb(s, str.c_str(), tid.c_str());
  // };

  // return run<gap_Status>(
  //   state,
  //   "error callback",
  //   [&] (surface::gap::State& state) -> gap_Status
  //   {
  //     state.notification_manager().on_error_callback(cpp_cb);
  //     return gap_ok;
  //   });
  return gap_ok;
}

gap_Status
gap_critical_callback(gap_State* state,
                      gap_critical_callback_t cb)
{
  return state->gap_critical_callback(state, cb);
}
/// Transaction getters.
#define DEFINE_TRANSACTION_GETTER(_type_, _field_, _transform_)                \
_type_                                                                       \
gap_transaction_ ## _field_(gap_State* state,                                \
                            uint32_t _id)                                    \
{                                                                            \
  assert(_id != surface::gap::null_id);                                      \
  return run<_type_>(                                                        \
    state,                                                                   \
    #_field_,                                                                \
    [&] (surface::gap::State& state) -> _type_                               \
    {                                                                        \
      auto res = _transform_(state.transactions().at(_id).data()->_field_);  \
      ELLE_DUMP("fetch "#_field_ ": %s", res);                               \
      return res;                                                            \
    });                                                                      \
    }                                                                        \
/**/

#define NO_TRANSFORM
#define GET_CSTR(_expr_) (_expr_).c_str()
#define GET_USER_ID(_expr_) (state.user_indexes().at(_expr_))

#define DEFINE_TRANSACTION_GETTER_STR(_field_)                                \
DEFINE_TRANSACTION_GETTER(char const*, _field_, GET_CSTR)                   \
/**/
#define DEFINE_TRANSACTION_GETTER_INT(_field_)                                \
DEFINE_TRANSACTION_GETTER(int, _field_, NO_TRANSFORM)                       \
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

DEFINE_TRANSACTION_GETTER(uint32_t, sender_id, GET_USER_ID)
DEFINE_TRANSACTION_GETTER_STR(sender_fullname)
DEFINE_TRANSACTION_GETTER_STR(sender_device_id)
DEFINE_TRANSACTION_GETTER(uint32_t, recipient_id, GET_USER_ID)
DEFINE_TRANSACTION_GETTER_STR(recipient_fullname)
DEFINE_TRANSACTION_GETTER_STR(recipient_device_id)
DEFINE_TRANSACTION_GETTER_STR(message)
DEFINE_TRANSACTION_GETTER(int64_t, files_count, NO_TRANSFORM)
DEFINE_TRANSACTION_GETTER(int64_t, total_size, NO_TRANSFORM)
DEFINE_TRANSACTION_GETTER_DOUBLE(ctime)
DEFINE_TRANSACTION_GETTER_DOUBLE(mtime)
DEFINE_TRANSACTION_GETTER_BOOL(is_directory)
// _transform_ is a cast from plasma::TransactionStatus

gap_TransactionStatus
gap_transaction_status(gap_State* state,
                      uint32_t const transaction_id)
{
  assert(state != nullptr);
  assert(transaction_id != surface::gap::null_id);

  return run<gap_TransactionStatus>(
    state,
    "transaction state",
    [&] (surface::gap::State& state)
    {
      return state.transactions().at(transaction_id).last_status();
    }
  );
}

char**
gap_transaction_files(gap_State* state,
                      uint32_t const transaction_id)
{
  assert(state != nullptr);
  assert(transaction_id != surface::gap::null_id);

  auto result = run<std::list<std::string>>(
    state,
    "transaction_files",
    [&] (surface::gap::State& state) {
      return state.transactions().at(transaction_id).data()->files;
    }
  );
  if (result.status() != gap_ok)
    return nullptr;
  return _cpp_stringlist_to_c_stringlist(result.value());
}

float
gap_transaction_progress(gap_State* state,
                         uint32_t id)
{
  assert(id != surface::gap::null_id);

  return run<float>(
    state,
    "progress",
    [&] (surface::gap::State& state) -> float
    {
      return state.transactions().at(id).progress();
    });
}

// - Notifications -----------------------------------------------------------

gap_Status
gap_pull_notifications(gap_State* state,
                       int count,
                       int offset)
{
  return run<gap_Status>(
    state,
    "pull",
    [&] (surface::gap::State& state) -> gap_Status
    {
      // state.notification_manager().pull(count, offset, false);
      return gap_ok;
    });
}

gap_Status
gap_pull_new_notifications(gap_State* state,
                           int count,
                           int offset)
{
  // return run<gap_Status>(
  //   state,
  //   "pull new",
  //   [&] (surface::gap::State& state) -> gap_Status
  //   {
  //     state.notification_manager().pull(count, offset, true);
  //     return gap_ok;
  //   });
  return gap_ok;
}

gap_Status
gap_notifications_read(gap_State* state)
{
  // return run<gap_Status>(
  //   state,
  //   "read",
  //   [&] (surface::gap::State& state) -> gap_Status
  //   {
  //     state.notification_manager().read();
  //     return gap_ok;
  //   });
  return gap_ok;
}

uint32_t*
gap_transactions(gap_State* state)
{
  assert(state != nullptr);

  auto ret = run<std::vector<uint32_t>>(
    state,
    "transactions",
    [&] (surface::gap::State& state) -> std::vector<uint32_t>
    {
      std::vector<uint32_t> values;
      auto const& trs = state.transactions();

      for(auto it = std::begin(trs); it != std::end(trs); ++it)
      {
        values.push_back(it->first);
      }

      return values;
    });

  if (ret.status() != gap_ok)
    return nullptr;

  return vector_to_pointer(ret.value());
}

void gap_transactions_free(uint32_t* transactions)
{
  ::free(transactions);
}

uint32_t
gap_send_files_by_email(gap_State* state,
                        char const* recipient_id,
                        char const* const* files,
                        char const* message)
{
  assert(recipient_id != nullptr);
  assert(files != nullptr);

  std::unordered_set<std::string> s;

  while (*files != nullptr)
  {
    s.insert(*files);
    ++files;
  }

  return run<uint32_t>(
    state,
    "send files",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.send_files(recipient_id, std::move(s), message);
      return 0;
    });
}

uint32_t
gap_send_files(gap_State* state,
               uint32_t id,
               char const* const* files,
               char const* message)
{
  assert(id != surface::gap::null_id);
  assert(files != nullptr);

  std::unordered_set<std::string> s;

  while (*files != nullptr)
  {
    s.insert(*files);
    ++files;
  }

  return run<uint32_t>(
    state,
    "send files",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.send_files(state.users().at(id).id,
                              std::move(s),
                              message);
    });
}

uint32_t
gap_cancel_transaction(gap_State* state,
                       uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<uint32_t>(
    state,
    "cancel transaction",
    [&] (surface::gap::State& state) -> uint32_t
    {
      state.transactions().at(id).cancel();
      return id;
    });
}

uint32_t
gap_reject_transaction(gap_State* state,
                       uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<uint32_t>(
    state,
    "reject transaction",
    [&] (surface::gap::State& state) -> uint32_t
    {
      state.transactions().at(id).reject();
      return id;
    });
}

uint32_t
gap_accept_transaction(gap_State* state,
                       uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<uint32_t>(
    state,
    "accept transaction",
    [&] (surface::gap::State& state) -> uint32_t
    {
      state.transactions().at(id).accept();
      return id;
    });
}

uint32_t
gap_join_transaction(gap_State* state,
                     uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<uint32_t>(
    state,
    "join transaction",
    [&] (surface::gap::State& state) -> uint32_t
    {
      state.transactions().at(id).join();
      return id;
    });
}

gap_Status
gap_set_output_dir(gap_State* state,
                   char const* output_path)
{
  assert(output_path != nullptr);

  return run<gap_Status>(
    state,
    "set output dir",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.output_dir(output_path);
      return gap_ok;
    });
}

char const*
gap_get_output_dir(gap_State* state)
{
  assert(state != nullptr);

  auto ret = run<std::string>(
    state,
    "join transaction",
    [&] (surface::gap::State& state) -> std::string
    {
      return state.output_dir();
    });

  return ret.value().c_str();
}

gap_Status
gap_send_user_report(char const* _user_name,
                     char const* _message,
                     char const* _file,
                     char const* _os_description)
{
  try
  {
    std::string const user_name{_user_name};
    std::string const os_description{_os_description};
    std::string const user_message{_message};
    std::string const file{_file};

    elle::crash::user_report(common::meta::protocol(),    // meta protocol
                             common::meta::host(),        // meta host
                             common::meta::port(),        // meta port
                             user_name,                   // user name
                             os_description,              // OS description
                             user_message,                // user message
                             file                         // file
      );
  }
  catch (...)
  {
    ELLE_WARN("cannot send user report: %s", elle::exception_string());
    return gap_api_error;
  }
  return gap_ok;
}

gap_Status
gap_send_last_crash_logs(char const* _user_name,
                         char const* _crash_report,
                         char const* _state_log,
                         char const* _os_description,
                         char const* _additional_info)
{
  try
  {
    std::string const user_name{_user_name};
    std::string const os_description{_os_description};
    std::string const additional_info{_additional_info};
    std::vector<std::string> files;
    files.push_back(std::string{_crash_report});
    files.push_back(std::string{_state_log});

    elle::crash::existing_report(common::meta::protocol(),// meta protocol
                                 common::meta::host(),    // meta host.
                                 common::meta::port(),    // meta port.
                                 files,                   // file list.
                                 user_name,               // user name.
                                 os_description,          // OS description.
                                 additional_info          // additional info.
      );
  }
  catch (...)
  {
    ELLE_WARN("cannot send crash reports: %s", elle::exception_string());
    return gap_api_error;
  }

  return gap_ok;
}
