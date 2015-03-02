#include <surface/gap/gap.hh>
#include <surface/gap/gap_bridge.hh>
#include <surface/gap/State.hh>
#include <surface/gap/Transaction.hh>
#include <surface/gap/onboarding/Transaction.hh>

#include <infinit/oracles/meta/Client.hh>

#include <common/common.hh>

#include <elle/log.hh>
#include <elle/elle.hh>
#include <elle/assert.hh>
#include <elle/finally.hh>
#include <elle/HttpClient.hh>
#include <elle/container/list.hh>
#include <CrashReporter.hh>

#include <reactor/scheduler.hh>
#include <reactor/thread.hh>
#include <reactor/network/proxy.hh>

#include <boost/range/join.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
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

gap_State* gap_new(bool production,
                   std::string const& home_dir,
                   std::string const& download_dir)
{
  try
  {
    gap_State* state = new gap_State(production, home_dir, download_dir);
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

void gap_free(gap_State* state)
{
  if (state == nullptr)
  {
    ELLE_WARN("State already destroyed");
  }
  else
  {
    delete state;
    state = nullptr;
    ELLE_LOG("State successfully destroyed");
  }
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

gap_Status
gap_debug(gap_State* state)
{
  (void) state;
  return gap_ok;
}

//- Authentication ----------------------------------------------------------
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
gap_internet_connection(gap_State* _state, bool connected)
{
  return run<gap_Status>(
    _state,
    "set internet connection",
    [&] (surface::gap::State& state)
    {
      state.internet_connection(connected);
      return gap_ok;
    }
  );
}

gap_Status
gap_set_proxy(gap_State* _state,
              gap_ProxyType type,
              std::string const& host,
              uint16_t port,
              std::string const& username,
              std::string const& password)
{
  return run<gap_Status>(
    _state,
    "set proxy",
    [&] (surface::gap::State& state) -> gap_Status
    {
      using ProxyType = reactor::network::ProxyType;
      using Proxy = reactor::network::Proxy;
      ProxyType type_;
      switch (type)
      {
        case gap_proxy_http:
          type_ = ProxyType::HTTP;
          break;
          case gap_proxy_https:
          type_ = ProxyType::HTTPS;
          break;
        case gap_proxy_socks:
          type_ = ProxyType::SOCKS;
          break;

        default:
          ELLE_ERR("unknown proxy type: %s", type);
          return gap_error;
      }
      Proxy proxy_(type_, host, port, username, password);
      state.set_proxy(proxy_);
      return gap_ok;
    });
}

gap_Status
gap_unset_proxy(gap_State* _state, gap_ProxyType type)
{
  return run<gap_Status>(
    _state,
    "unset proxy",
    [&] (surface::gap::State& state) -> gap_Status
    {
      using ProxyType = reactor::network::ProxyType;
      ProxyType type_;
      switch (type)
      {
        case gap_proxy_http:
          type_ = ProxyType::HTTP;
          break;
        case gap_proxy_https:
          type_ = ProxyType::HTTPS;
          break;
        case gap_proxy_socks:
          type_ = ProxyType::SOCKS;
          break;
      }
      state.unset_proxy(type_);
      return gap_ok;
    });
}

void
gap_clean_state(gap_State* state)
{
  run<gap_Status>(
    state,
    "flush state",
    [&] (surface::gap::State& state)
    {
      state.clean();
      return gap_ok;
    });
}

gap_Status
gap_login(gap_State* _state,
          char const* email,
          char const* password)
{
  assert(email != nullptr);
  assert(password != nullptr);

  return run<gap_Status>(_state,
                         "login",
                         [&] (surface::gap::State& state) -> gap_Status
                         {
                           state.login(email, password);
                           return gap_ok;
                         });
}

std::unordered_map<std::string, std::string>
gap_fetch_features(gap_State* _state)
{
  return run<std::unordered_map<std::string, std::string>>(
    _state,
    "fetch features",
    [&] (surface::gap::State& state) ->
      std::unordered_map<std::string, std::string>
    {
      return state.configuration().features;
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
             std::string const& fullname,
             std::string const& email,
             std::string const& hashed_password)
{
  auto ret = run<gap_Status>(
    state,
    "register",
    [&] (surface::gap::State& state) -> gap_Status
    {
     state.register_(fullname, email, hashed_password);
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
std::string
gap_self_email(gap_State* state)
{
  return run<std::string>(state,
                          "self email",
                          [&] (surface::gap::State& state) -> std::string
                          {
                            return state.me().email;
                          });
}

gap_Status
gap_set_self_email(gap_State* state,
                   std::string const& email,
                   std::string const& password)
{
  return run<gap_Status>(state,
                         "set self email",
                         [&] (surface::gap::State& state) -> gap_Status
                        {
                          state.meta().change_email(email, password);
                          return gap_ok;
                        });
}

std::string
gap_self_fullname(gap_State* state)
{
  return run<std::string>(state,
                          "self fullname",
                          [&] (surface::gap::State& state) -> std::string
                          {
                            return state.me().fullname;
                          });
}

gap_Status
gap_set_self_fullname(gap_State* state, std::string const& fullname)
{
  return run<gap_Status>(state,
                          "set self fullname",
                          [&] (surface::gap::State& state) -> gap_Status
                          {
                            auto handle = state.me().handle;
                            state.meta().edit_user(fullname, handle);
                            state.update_me();
                            return gap_ok;
                          });
}

std::string
gap_self_handle(gap_State* state)
{
  return run<std::string>(state,
                          "self handle",
                          [&] (surface::gap::State& state) -> std::string
                          {
                            return state.me().handle;
                          });
}

gap_Status
gap_set_self_handle(gap_State* state, std::string const& handle)
{
  return run<gap_Status>(state,
                         "set self handle",
                         [&] (surface::gap::State& state) -> gap_Status
                         {
                           auto fullname = state.me().fullname;
                           state.meta().edit_user(fullname, handle);
                           state.update_me();
                           return gap_ok;
                         });
}

gap_Status
gap_change_password(gap_State* state,
                    std::string const& old_password,
                    std::string const& new_password)
{
  return run<gap_Status>(
    state,
    "change password",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.change_password(old_password,  new_password);
      return gap_ok;
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

gap_Bool
gap_user_ghost(gap_State* state, uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<gap_Bool>(state,
                       "user ghost",
                       [&] (surface::gap::State& state)
                       {
                        auto const& user = state.user(id);
                        return user.ghost();
                       });
  return false;
}

gap_Bool
gap_user_deleted(gap_State* state, uint32_t id)
{
  assert(id != surface::gap::null_id);
  return run<gap_Bool>(state,
                       "user deleted",
                       [&] (surface::gap::State& state)
                       {
                        auto const& user = state.user(id);
                        return user.deleted();
                       });
  return false;
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

std::string
gap_self_device_id(gap_State* state)
{
  return run<std::string>(state,
                          "self device id",
                          [&] (surface::gap::State& state) -> std::string
                          {
                            return state.device().id;
                          });
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
    "user avatar",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto res = state.user_icon(state.user(user_id).id);

      *data = (void*) res.contents();
      *size = res.size();

      return gap_ok;
    });
}

void
gap_refresh_avatar(gap_State* state, uint32_t user_id)
{
  run<gap_Status>(
    state,
    "refresh user avatar",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.user_icon_refresh(user_id);
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

std::vector<uint32_t>
gap_users_search(gap_State* state, std::string const& text)
{
  assert(text.size() != 0);
  auto ret = run<std::vector<uint32_t>>(
    state,
    "users search",
    [&] (surface::gap::State& state) -> std::vector<uint32_t>
    {
      return state.users_search(text);
    });
  return ret.value();
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
        return (gap_UserStatus) state.users().at(id).online();
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
gap_deleted_swagger_callback(gap_State* state,
                             gap_deleted_swagger_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::DeletedSwaggerNotification const& notif)
    {
      cb(notif.id);
    };

  return run<gap_Status>(
    state,
    "deleted swagger callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::DeletedSwaggerNotification>(cpp_cb);
      return gap_ok;
    });
}

gap_Status
gap_deleted_favorite_callback(gap_State* state,
                             gap_deleted_swagger_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::State::DeletedFavoriteNotification const& notif)
    {
      cb(notif.id);
    };

  return run<gap_Status>(
    state,
    "deleted favorite callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::DeletedFavoriteNotification>(cpp_cb);
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
      cb(notif.status, notif.still_trying, notif.last_error);
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
gap_link_callback(
  gap_State* state,
  std::function<void (surface::gap::LinkTransaction const&)> const& callback)
{
  return run<gap_Status>(
    state,
    "link callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::LinkTransaction>(callback);
      return gap_ok;
    });
}

/// Recipient changeh callback.
gap_Status
gap_transaction_recipient_changed_callback(
  gap_State* state,
  gap_recipient_changed_callback_t cb)
{
  auto cpp_cb = [cb] (surface::gap::Transaction::RecipientChangedNotification const& notif)
  {
    cb(notif.id, notif.recipient_id);
  };

  return run<gap_Status>(
    state,
    "recipient changed callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::Transaction::RecipientChangedNotification>(cpp_cb);
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
#define DEFINE_TRANSACTION_GETTER(_type_, _field_, _transform_)               \
_type_                                                                        \
gap_transaction_ ## _field_(gap_State* state,                                 \
                            uint32_t _id)                                     \
{                                                                             \
  assert(_id != surface::gap::null_id);                                       \
  return run<_type_>(                                                         \
    state,                                                                    \
    #_field_,                                                                 \
    [&] (surface::gap::State& state) -> _type_                                \
    {                                                                         \
      auto peer_data =                                                        \
        std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(         \
          state.transactions().at(_id)->data());                              \
      ELLE_ASSERT(peer_data != nullptr);                                      \
      auto res = _transform_(peer_data->_field_);                             \
      ELLE_DUMP("fetch "#_field_ ": %s", res);                                \
      return res;                                                             \
    });                                                                       \
}                                                                             \
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
      return state.transactions().at(transaction_id)->status();
    }
  );
}

bool
gap_transaction_is_final(gap_State* state,
                         uint32_t const transaction_id)
{
  assert(state != nullptr);
  assert(transaction_id != surface::gap::null_id);

  return run<bool>(
    state,
    "transaction is final",
    [&] (surface::gap::State& state)
    {
      return state.transactions().at(transaction_id)->final();
    }
  );
}

bool
gap_transaction_concern_device(gap_State* state,
                               uint32_t const transaction_id,
                               bool true_if_empty_recipient)
{
  assert(state != nullptr);
  assert(transaction_id != surface::gap::null_id);

  return run<bool>(
    state,
    "transaction concerns our device",
    [&] (surface::gap::State& state)
    {
      auto const& tr = state.transactions().at(transaction_id);
      using namespace infinit::oracles;
      if (auto data = std::dynamic_pointer_cast<PeerTransaction>(tr->data()))
      {
        return
          (data->recipient_id == state.me().id &&
           ((true_if_empty_recipient && data->recipient_device_id.empty()) ||
            data->recipient_device_id == state.device().id)) ||
          (data->sender_id == state.me().id &&
           data->sender_device_id == state.device().id);
      }
      else if (auto data = std::dynamic_pointer_cast<LinkTransaction>(tr->data()))
      {
        return (data->sender_id == state.me().id &&
                data->sender_device_id == state.device().id);
      }
      else
      {
        return false;
      }
    }
  );
}

uint32_t
gap_transaction_canceler_id(gap_State* state, uint32_t id)
{
  return run<uint32_t>(
    state,
    "transaction canceler",
    [&] (surface::gap::State& state)
    {
      uint32_t res = 0;
      auto data = std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
        state.transactions().at(id)->data());
      if (!data->canceler.user_id.empty())
        res = state.user_indexes().at(data->canceler.user_id);
      return res;
    });
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
    [&] (surface::gap::State& state)
    {
      using namespace infinit::oracles;
      auto peer_data =
        std::dynamic_pointer_cast<PeerTransaction>(
          state.transactions().at(transaction_id)->data());
      ELLE_ASSERT(peer_data != nullptr);
      return peer_data->files;
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
  assert(state != nullptr);
  assert(id != surface::gap::null_id);

  return run<float>(
    state,
    "progress",
    [&] (surface::gap::State& state) -> float
    {
      auto it = state.transactions().find(id);
      if (it == state.transactions().end())
      {
        ELLE_ERR("gap_transaction_progress: transaction %s doesn't exist", id);
        return 0;
      }
      return it->second->progress();
    });
}

bool
gap_is_p2p_transaction(gap_State* state, uint32_t id)
{
  return run<bool>(
    state,
    "is p2p transaction",
    [&] (surface::gap::State& state) -> bool
    {
      auto data =
        std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
          state.transactions().at(id)->data());
      if (data == nullptr)
        return false;
      else
        return true;
    });
}

bool
gap_is_link_transaction(gap_State* state, uint32_t id)
{
  return run<bool>(
    state,
    "is link transaction",
    [&] (surface::gap::State& state) -> bool
    {
      auto data =
        std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
          state.transactions().at(id)->data());
      if (data == nullptr)
        return false;
      else
        return true;
    });
}

uint32_t
gap_create_link_transaction(gap_State* state,
                            std::vector<std::string> const& files,
                            std::string const& message)
{
  ELLE_ASSERT(state != nullptr);
  return run<uint32_t>(
    state,
    "create link",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.create_link(files, message);
    });
}

surface::gap::LinkTransaction
gap_link_transaction_by_id(gap_State* state,
                           uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<surface::gap::LinkTransaction>(
    state,
    "link transaction by id",
    [&] (surface::gap::State& state) -> surface::gap::LinkTransaction
  {
    auto data =
      std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
        state.transactions().at(id)->data());
    ELLE_ASSERT(data != nullptr);
    auto status = state.transactions().at(id)->status();
    auto txn = surface::gap::LinkTransaction(id,
                                             data->name,
                                             data->mtime,
                                             data->share_link,
                                             data->click_count,
                                             status);
    return txn;
  });
}

std::vector<surface::gap::LinkTransaction>
gap_link_transactions(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  auto ret = run<std::vector<surface::gap::LinkTransaction>>(
    state,
    "link transactions",
    [&]
    (surface::gap::State& state) -> std::vector<surface::gap::LinkTransaction>
    {
      std::vector<surface::gap::LinkTransaction> values;
      auto const& trs = state.transactions();
      for(auto it = std::begin(trs); it != std::end(trs); ++it)
      {
        auto data =
          std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
            it->second->data());
        if (data != nullptr)
        {
          auto status = state.transactions().at(it->first)->status();
          auto txn = surface::gap::LinkTransaction(it->first,
                                                   data->name,
                                                   data->mtime,
                                                   data->share_link,
                                                   data->click_count,
                                                   status);
          values.push_back(txn);
        }
      }
      return values;
    });
  return ret;
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
        auto peer_data =
          std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
            it->second->data());
        if (peer_data != nullptr)
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
  std::vector<std::string> files_cxx;
  while (*files != nullptr)
  {
    files_cxx.push_back(*files);
    ++files;
  }
  return gap_send_files_by_email(state, recipient_id, files_cxx, message);
}

uint32_t
gap_send_files_by_email(gap_State* state,
                        std::string const& email,
                        std::vector<std::string> const& files,
                        std::string const& message)
{
  return run<uint32_t>(
    state,
    "send files",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.send_files(email, std::move(files), message);
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
  std::vector<std::string> files_cxx;
  while (*files != nullptr)
  {
    files_cxx.push_back(*files);
    ++files;
  }
  return gap_send_files(state, id, files_cxx, message);
}

uint32_t
gap_send_files(gap_State* state,
               uint32_t id,
               std::vector<std::string> const& files,
               std::string const& message)
{
  return run<uint32_t>(
    state,
    "send files",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.send_files(state.users().at(id).id,
                              std::move(files),
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
      state.transactions().at(id)->cancel(true);
      return id;
    });
}

uint32_t
gap_delete_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  return run<uint32_t>(
    state,
    "delete link",
    [&] (surface::gap::State& state) -> uint32_t
    {
      state.transactions().at(id)->delete_();
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
      state.transactions().at(id)->reject();
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
      state.transactions().at(id)->accept();
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
      state.transactions().at(id)->join();
      return id;
    });
}

gap_Status
gap_set_output_dir(gap_State* state,
                   std::string const& output_path,
                   bool fallback)
{
  return run<gap_Status>(
    state,
    "set output dir",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.set_output_dir(output_path, fallback);
      return gap_ok;
    });
}

std::string
gap_get_output_dir(gap_State* state)
{
  assert(state != nullptr);

  return run<std::string>(
    state,
    "join transaction",
    [&] (surface::gap::State& state) -> std::string
    {
      return state.output_dir();
    });
}

uint32_t
gap_onboarding_receive_transaction(
  gap_State* state, std::string const& file_path, uint32_t transfer_time_sec)
{
  auto transfer_time =
    reactor::Duration(boost::posix_time::seconds(transfer_time_sec));
  return run<uint32_t>(
    state,
    "start reception onboarding",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.start_onboarding(file_path, transfer_time);
    });
}

/// Change the peer connection status.
gap_Status
gap_onboarding_set_peer_status(gap_State* state,
                               uint32_t transaction_id,
                               bool status)
{
  return run<gap_Status>(
    state,
    "change onboarding peer status",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto& tr = state.transactions().at(transaction_id);
      if (!dynamic_cast<surface::gap::onboarding::Transaction*>(tr.get()))
        return gap_error;
      // FIXME
      // tr->peer_connection_status(status);
      return gap_ok;
    });
}

/// Change the peer availability status.
gap_Status
gap_onboarding_set_peer_availability(gap_State* state,
                                     uint32_t transaction_id,
                                     bool status)
{
  return run<gap_Status>(
    state,
    "change onboarding peer availability",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto& tr = state.transactions().at(transaction_id);
      if (!dynamic_cast<surface::gap::onboarding::Transaction*>(tr.get()))
        return gap_error;
      if (status)
        tr->notify_peer_reachable({}, {});
      else
        tr->notify_peer_unreachable();
      return gap_ok;
    });
}

/// Force transfer deconnection.
gap_Status
gap_onboarding_interrupt_transfer(gap_State* state,
                                  uint32_t transaction_id)
{
  return run<gap_Status>(
    state,
    "interrupt onboarding transfer",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto const& tr = state.transactions().at(transaction_id);
      if (!dynamic_cast<surface::gap::onboarding::Transaction*>(tr.get()))
        return gap_error;
      tr->interrupt();
      return gap_ok;
    });
}

// Metrics.
gap_Status
gap_send_metric(gap_State* state,
                UIMetricsType metric,
                std::unordered_map<std::string, std::string> additional)
{
  return run<gap_Status>(
    state,
    "gap send metrics",
    [&] (surface::gap::State& state)
    {
      switch (metric)
      {
        // Adding files.
        case UIMetrics_AddFilesSendView:
          state.metrics_reporter()->ui("add files", "send view", additional);
          break;
        case UIMetrics_AddFilesContextual:
          state.metrics_reporter()->ui("add files", "contextual", additional);
          break;
        case UIMetrics_AddFilesMenu:
          state.metrics_reporter()->ui("add files", "menu", additional);
          break;
        case UIMetrics_AddFilesDropOnSendView:
          state.metrics_reporter()->ui("add files", "drop", additional);
          break;
        case UIMetrics_AddFilesDropOnIcon:
          state.metrics_reporter()->ui("add files", "status icon drop", additional);
          break;
        // Show dock.
        case UIMetrics_OpenPanelIcon:
          state.metrics_reporter()->ui("open infinit", "status icon", additional);
          break;
        case UIMetrics_OpenPanelMenu:
          state.metrics_reporter()->ui("open infinit", "menu", additional);
          break;
        case UIMetrics_OpenPanelOtherInstance:
          state.metrics_reporter()->ui("open infinit", "other instance", additional);
          break;
        // Actions on Transactions.
        case UIMetrics_ConversationAccept:
          state.metrics_reporter()->ui("accept", "conversation view", additional);
          break;
        case UIMetrics_ConversationCancel:
          state.metrics_reporter()->ui("cancel", "conversation view", additional);
          break;
        case UIMetrics_ConversationReject:
          state.metrics_reporter()->ui("rejecte", "conversation view", additional);
          break;
        // Actions on Links.
        case UIMetrics_MainCopyLink:
          state.metrics_reporter()->ui("copy link", "main view", additional);
          break;
        case UIMetrics_MainOpenLink:
          state.metrics_reporter()->ui("open link", "main view", additional);
          break;
        case UIMetrics_MainDeleteLink:
          state.metrics_reporter()->ui("delete link", "main view", additional);
          break;
        // Way to start transaction.
        case UIMetrics_FavouritesPersonDrop:
          state.metrics_reporter()->ui("create transaction", "favourites", additional);
          break;
        case UIMetrics_ContextualSend:
          state.metrics_reporter()->ui("create transaction", "contextual", additional);
          break;
        case UIMetrics_ConversationSend:
          state.metrics_reporter()->ui("create transaction", "conversation view", additional);
          break;
        // Way to start a link.
        case UIMetrics_FavouritesLinkDrop:
          state.metrics_reporter()->ui("create link", "favourites", additional);
          break;
        case UIMetrics_ContextualLink:
          state.metrics_reporter()->ui("create link", "contextual", additional);
          break;
        case UIMetrics_StatusIconLinkDrop:
          state.metrics_reporter()->ui("create link", "status icon drop", additional);
          break;
        // Validate transfer draft.
        case UIMetrics_SendCreateTransaction:
          state.metrics_reporter()->ui("create transaction", "send view", additional);
          break;
        case UIMetrics_SendCreateLink:
          state.metrics_reporter()->ui("create link", "send view", additional);
          break;
        // Peer selection.
        case UIMetrics_SelectPeer:
          state.metrics_reporter()->ui("select peer", "send view", additional);
          break;
        case UIMetrics_UnselectPeer:
          state.metrics_reporter()->ui("unselect peer", "send view", additional);
          break;
        // Screenshots.
        case UIMetrics_UploadScreenshot:
          state.metrics_reporter()->ui("upload screenshots", "automatic", additional);
          break;
        case UIMetrics_ScreenshotModalNo:
          state.metrics_reporter()->ui("upload screenshots", "no", additional);
          break;
        case UIMetrics_ScreenshotModalYes:
          state.metrics_reporter()->ui("upload screenshots", "yes", additional);
          break;
        // Navigation.
        case UIMetrics_MainSend:
          state.metrics_reporter()->ui("send", "main", additional);
          break;
        case UIMetrics_MainPeople:
          state.metrics_reporter()->ui("people", "main", additional);
          break;
        case UIMetrics_MainLinks:
          state.metrics_reporter()->ui("link", "main", additional);
          break;
        case UIMetrics_SendTrash:
          state.metrics_reporter()->ui("cancel", "send view", additional);
          break;
        // Address book.
        case UIMetrics_HaveAddressbookAccess:
          state.metrics_reporter()->ui("addressbook", "yes", additional);
          break;
        case UIMetrics_NoAdressbookAccess:
          state.metrics_reporter()->ui("addressbook", "no", additional);
          break;
        // Other.
        case UIMetrics_DesktopNotification:
          state.metrics_reporter()->ui("desktop notification", "click", additional);
          break;
        case UIMetrics_Preferences:
          state.metrics_reporter()->ui("preference", "click", additional);
          break;
        default:
          elle::unreachable();
      }
      return gap_ok;
    });
}

gap_Status
gap_send_user_report(gap_State* state,
                     std::string const& user_name,
                     std::string const& message,
                     std::string const& file)
{
  // In order to avoid blocking the GUI, let's create a disposable thread and
  // let it go.
  // XXX: The gap_Status inside catch_to_gap_status is useless.
  bool disposable = true;
  new reactor::Thread(
    state->scheduler(),
    "send user report",
    [=] ()
    {
      catch_to_gap_status<gap_Status>(
        [=] ()
        {
          auto& _state = state->state();
          elle::crash::user_report(_state.meta(false).protocol(),
                                   _state.meta(false).host(),
                                   _state.meta(false).port(),
                                   _state.home(),
                                   user_name,
                                   message,
                                   file);
          return gap_ok;
        }, "send user report");
    }, disposable);
  return gap_ok;
}

gap_Status
gap_send_last_crash_logs(gap_State* state,
                         std::string const& user_name,
                         std::string const& crash_report,
                         std::string const& state_log,
                         std::string const& additional_info)
{
  // In order to avoid blocking the GUI, let's create a disposable thread and
  // let it go.
  // XXX: The gap_Status inside catch_to_gap_status is useless.
  bool disposable = true;
  new reactor::Thread(
    state->scheduler(),
    "send last crash report",
    [=] ()
    {
      catch_to_gap_status<gap_Status>(
        [=] ()
        {
          std::vector<std::string> files;
          files.push_back(crash_report);
          files.push_back(state_log);

          auto& _state = state->state();
          _state.metrics_reporter()->user_crashed();
          elle::crash::existing_report(_state.meta(false).protocol(),
                                       _state.meta(false).host(),
                                       _state.meta(false).port(),
                                       files,
                                       user_name,
                                       additional_info);
          return gap_ok;
        }, "send last crash report");
    }, disposable);
  return gap_ok;
}
