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

/// - gap ctor & dtor -----------------------------------------------------

gap_State*
gap_new(bool production,
        std::string const& download_dir,
        std::string const& persistent_config_dir,
        std::string const& non_persistent_config_dir,
        bool enable_mirroring,
        uint64_t max_mirroring_size)
{
  try
  {
    gap_State* state = new gap_State(production,
                                     download_dir,
                                     persistent_config_dir,
                                     non_persistent_config_dir,
                                     enable_mirroring,
                                     max_mirroring_size);
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

void
gap_free(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
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

uint32_t
gap_null()
{
  return surface::gap::null_id;
}

gap_Status
gap_internet_connection(gap_State* state, bool connected)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "set internet connection",
    [&] (surface::gap::State& state)
    {
      state.internet_connection(connected);
      return gap_ok;
    }
  );
}

gap_Status
gap_set_proxy(gap_State* state,
              gap_ProxyType type,
              std::string const& host,
              uint16_t port,
              std::string const& username,
              std::string const& password)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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
gap_unset_proxy(gap_State* state, gap_ProxyType type)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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

//- Authentication -------------------------------------------------------------
void
gap_clean_state(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
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
gap_login(gap_State* state,
          std::string const& email,
          std::string const& password,
          boost::optional<std::string const&> device_push_token,
          reactor::DurationOpt timeout)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "login",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.login(email, password, timeout, device_push_token);
      return gap_ok;
    });
}

std::unordered_map<std::string, std::string>
gap_fetch_features(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<std::unordered_map<std::string, std::string>>(
    state,
    "fetch features",
    [&] (surface::gap::State& state) ->
      std::unordered_map<std::string, std::string>
    {
      return state.configuration().features;
    });
}

bool
gap_logged_in(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<bool>(
    state,
    "logged",
    [&] (surface::gap::State& state)
    {
      return state.logged_in();
    });
}

gap_Status
gap_logout(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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
             std::string const& password,
             boost::optional<std::string const&> device_push_token)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "register",
    [&] (surface::gap::State& state) -> gap_Status
    {
     state.register_(fullname, email, password, device_push_token);
     return gap_ok;
    });
}

gap_Status
gap_poll(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
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
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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
gap_set_device_name(gap_State* state, std::string const& name)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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
  ELLE_ASSERT(state != nullptr);
  return run<std::string>(
    state,
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
  return run<gap_Status>(
    state,
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
  ELLE_ASSERT(state != nullptr);
  return run<std::string>(
    state,
    "self fullname",
    [&] (surface::gap::State& state) -> std::string
    {
      return state.me().fullname;
    });
}

gap_Status
gap_set_self_fullname(gap_State* state, std::string const& fullname)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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
  ELLE_ASSERT(state != nullptr);
  return run<std::string>(
    state,
    "self handle",
    [&] (surface::gap::State& state) -> std::string
    {
      return state.me().handle;
    });
}

gap_Status
gap_set_self_handle(gap_State* state, std::string const& handle)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
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
  ELLE_ASSERT(state != nullptr);
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
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(state != nullptr);
  return run<uint32_t>(
    state,
    "self id",
    [&] (surface::gap::State& state) -> uint32_t
    {
      return state.user_indexes().at(state.me().id);
    });
}

/// Publish avatar to meta.
gap_Status
gap_update_avatar(gap_State* state, void const* data, size_t size)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(data != nullptr);
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

gap_Status
gap_user_by_id(gap_State* state, uint32_t id, surface::gap::User& res)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "user by id",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto const& user = state.user(id);
      res = surface::gap::User(
        state.user_indexes().at(user.id),
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        state.is_swagger(id),
        user.deleted(),
        user.ghost());
      return gap_ok;
    });
}

gap_Status
gap_user_by_meta_id(gap_State* state,
                    std::string const& meta_id,
                    surface::gap::User& res)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(!meta_id.empty());
  return run<gap_Status>(
    state,
    "user by meta id",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto const& user = state.user(meta_id);
      uint32_t state_id = state.user_indexes().at(user.id);
      res = surface::gap::User(
        state_id,
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        state.is_swagger(state_id),
        user.deleted(),
        user.ghost());
      return gap_ok;
    });
}

std::string
gap_self_device_id(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<std::string>(
    state,
    "self device id",
    [&] (surface::gap::State& state) -> std::string
    {
      return state.device().id;
    });
}

gap_Status
gap_avatar(gap_State* state, uint32_t id, void** data, size_t* size)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "user avatar",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto res = state.user_icon(state.user(id).id);
      *data = (void*) res.contents();
      *size = res.size();
      return gap_ok;
    });
}

gap_Status
gap_refresh_avatar(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "refresh user avatar",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.user_icon_refresh(id);
      return gap_ok;
    });
}

gap_Status
gap_user_by_email(gap_State* state,
                  std::string const& email,
                  surface::gap::User& res)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "user by email",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto user = state.user(email, true);
      uint32_t numeric_id = state.user_indexes().at(user.id);
      res = surface::gap::User(
        numeric_id,
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        state.is_swagger(numeric_id),
        user.deleted(),
        user.ghost());
      return gap_ok;
    });
}

gap_Status
gap_user_by_handle(gap_State* state,
                   std::string const& handle,
                   surface::gap::User& res)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "user by handle",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto user = state.user_from_handle(handle);
      uint32_t numeric_id = state.user_indexes().at(user.id);
      res = surface::gap::User(
        numeric_id,
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        state.is_swagger(numeric_id),
        user.deleted(),
        user.ghost());
      return gap_ok;
    });
}

gap_Status
gap_users_search(gap_State* state,
                 std::string const& text,
                 std::vector<surface::gap::User>& res)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "users search",
    [&] (surface::gap::State& state) -> gap_Status
    {
      res = state.users_search(text);
      return gap_ok;
    });
}

gap_Status
gap_users_by_emails(gap_State* state,
                    std::vector<std::string> const& emails,
                    std::unordered_map<std::string, surface::gap::User>& res)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(emails.size() != 0);
  return run<gap_Status>(
    state,
    "emails and users",
    [&] (surface::gap::State& state) -> gap_Status
    {
      res = state.users_by_emails(emails);
      return gap_ok;
    });
}

gap_UserStatus
gap_user_status(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_UserStatus>(
    state,
    "user status",
    [&] (surface::gap::State& state) -> gap_UserStatus
    {
      return (gap_UserStatus) state.users().at(id).online();
    });
}

gap_Status
gap_swaggers(gap_State* state, std::vector<surface::gap::User>& res)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "swaggers",
    [&] (surface::gap::State& state) -> gap_Status
    {
      for (uint32_t user_id: state.swaggers())
      {
        auto user = state.user(user_id);
        surface::gap::User ret_user(
          user_id,
          user.online(),
          user.fullname,
          user.handle,
          user.id,
          state.is_swagger(user_id),
          user.deleted(),
          user.ghost());
        res.push_back(ret_user);
      }
      return gap_ok;
    });
}

gap_Status
gap_favorites(gap_State* state, std::vector<uint32_t>& res)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "favorites",
    [&] (surface::gap::State& state) -> gap_Status
    {
      for (std::string const& fav: state.me().favorites)
      {
        state.user(fav); // update user indexes
        res.push_back(state.user_indexes().at(fav));
      }
      return gap_ok;
    });
}

gap_Status
gap_favorite(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "favorite",
    [&] (surface::gap::State& state)
    {
      std::string meta_id = state.users().at(id).id;
      std::list<std::string>& favorites =
        const_cast<std::list<std::string>&>(state.me().favorites);
      if (std::find(favorites.begin(),
                    favorites.end(),
                    meta_id) == favorites.end())
      {
        favorites.push_back(meta_id);
        state.meta().favorite(meta_id);
        if (state.metrics_reporter())
          state.metrics_reporter()->user_favorite(meta_id);
      }
      return gap_ok;
    });
}

gap_Status
gap_unfavorite(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "favorite",
    [&] (surface::gap::State& state)
    {
      std::string meta_id = state.users().at(id).id;
      // XXX Should be notification driven
      std::list<std::string>& favorites =
        const_cast<std::list<std::string>&>(state.me().favorites);
      auto it = std::find(favorites.begin(),
                          favorites.end(),
                          meta_id);
      if (it != favorites.end())
      {
        favorites.erase(it);
        state.meta().unfavorite(meta_id);
        if (state.metrics_reporter())
          state.metrics_reporter()->user_unfavorite(meta_id);
      }
      return gap_ok;
    });
}

bool
gap_is_favorite(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<bool>(
    state,
    "is_favorite",
    [&] (surface::gap::State& state)
    {
      std::string meta_id = state.users().at(id).id;
      auto it = std::find(state.me().favorites.begin(),
                          state.me().favorites.end(),
                          meta_id);
      return (it != state.me().favorites.end());
    });
}

// - Trophonius ----------------------------------------------------------------

gap_Status
gap_new_swagger_callback(
  gap_State* state,
  std::function<void (surface::gap::User const&)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "new swagger callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::User>(callback);
      return gap_ok;
    });
}

gap_Status
gap_deleted_swagger_callback(
  gap_State* state,
  std::function<void (uint32_t id)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  auto cb_wrapper =
    [callback]
    (surface::gap::State::DeletedSwaggerNotification const& notification)
    {
      callback(notification.id);
    };
  return run<gap_Status>(
    state,
    "deleted swagger callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<
        surface::gap::State::DeletedSwaggerNotification>(cb_wrapper);
      return gap_ok;
    });
}

gap_Status
gap_deleted_favorite_callback(
  gap_State* state,
  std::function<void (uint32_t id)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  auto cb_wrapper =
    [callback]
    (surface::gap::State::DeletedFavoriteNotification const& notification)
    {
      callback(notification.id);
    };
  return run<gap_Status>(
    state,
    "deleted favorite callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<
        surface::gap::State::DeletedFavoriteNotification>(cb_wrapper);
      return gap_ok;
    });
}

gap_Status
gap_user_status_callback(
  gap_State* state,
  std::function<void (uint32_t id, bool status)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  auto cb_wrapper =
    [callback]
    (surface::gap::State::UserStatusNotification const& notification)
    {
      callback(notification.id, notification.status);
    };
  return run<gap_Status>(
    state,
    "user callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<
        surface::gap::State::UserStatusNotification>(cb_wrapper);
      return gap_ok;
    });
}

gap_Status
gap_avatar_available_callback(
  gap_State* state,
  std::function<void (uint32_t id)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  auto cb_wrapper =
    [callback]
    (surface::gap::State::AvatarAvailableNotification const& notification)
    {
      callback(notification.id);
    };
  return run<gap_Status>(
    state,
    "avatar available callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<
        surface::gap::State::AvatarAvailableNotification>(cb_wrapper);
      return gap_ok;
    });
}

gap_Status
gap_connection_callback(
  gap_State* state,
  std::function<void (bool status,
                      bool still_retrying,
                      std::string const& last_error)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  auto cb_wrapper =
    [callback]
    (surface::gap::State::ConnectionStatus const& notification)
    {
      callback(notification.status,
               notification.still_trying,
               notification.last_error);
    };
  return run<gap_Status>(
    state,
    "connection status callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::State::ConnectionStatus>(cb_wrapper);
      return gap_ok;
    });
}

gap_Status
gap_peer_transaction_callback(
  gap_State* state,
  std::function<void (surface::gap::PeerTransaction const&)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "transaction callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::PeerTransaction>(callback);
      return gap_ok;
    });
}

gap_Status
gap_link_callback(
  gap_State* state,
  std::function<void (surface::gap::LinkTransaction const&)> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "link callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<surface::gap::LinkTransaction>(callback);
      return gap_ok;
    });
}

gap_Status
gap_critical_callback(gap_State* state, std::function<void ()> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  return state->gap_critical_callback(state, callback);
}

gap_Status
gap_peer_transaction_by_id(gap_State* state,
                           uint32_t id,
                           surface::gap::PeerTransaction& res)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "peer transaction (id)",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto peer_data =
        std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
          state.transactions().at(id)->data());
      ELLE_ASSERT(peer_data != nullptr);
      auto status = state.transactions().at(id)->status();
      res = surface::gap::PeerTransaction(
        id,
        status,
        state.user_indexes().at(peer_data->sender_id),
        peer_data->sender_device_id,
        state.user_indexes().at(peer_data->recipient_id),
        peer_data->recipient_device_id,
        peer_data->mtime,
        peer_data->files,
        peer_data->total_size,
        peer_data->is_directory,
        peer_data->message,
        peer_data->canceler,
        peer_data->id);
      return gap_ok;
    });
}

bool
gap_transaction_is_final(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<bool>(
    state,
    "transaction is final",
    [&] (surface::gap::State& state)
    {
      return state.transactions().at(id)->final();
    }
  );
}

bool
gap_transaction_concern_device(gap_State* state,
                               uint32_t id,
                               bool true_if_empty_recipient)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<bool>(
    state,
    "transaction concerns our device",
    [&] (surface::gap::State& state)
    {
      auto const& tr = state.transactions().at(id);
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

float
gap_transaction_progress(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
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
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
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

gap_Status
gap_link_transaction_by_id(gap_State* state,
                           uint32_t id,
                           surface::gap::LinkTransaction& res)
{
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "link transaction by id",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto data =
        std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
          state.transactions().at(id)->data());
      ELLE_ASSERT(data != nullptr);
      auto status = state.transactions().at(id)->status();
      res = surface::gap::LinkTransaction(id,
                                          data->name,
                                          data->mtime,
                                          data->share_link,
                                          data->click_count,
                                          status,
                                          data->sender_device_id,
                                          data->message,
                                          data->id);
      return gap_ok;
    });
}

gap_Status
gap_link_transactions(gap_State* state,
                      std::vector<surface::gap::LinkTransaction>& res)
{
  ELLE_ASSERT(state != nullptr);
  auto ret = run<gap_Status>(
    state,
    "link transactions",
    [&]
    (surface::gap::State& state) -> gap_Status
    {
      auto const& trs = state.transactions();
      for(auto it = std::begin(trs); it != std::end(trs); ++it)
      {
        auto link_data =
          std::dynamic_pointer_cast<infinit::oracles::LinkTransaction>(
            it->second->data());
        if (link_data != nullptr)
        {
          auto status = state.transactions().at(it->first)->status();
          auto txn = surface::gap::LinkTransaction(it->first,
                                                   link_data->name,
                                                   link_data->mtime,
                                                   link_data->share_link,
                                                   link_data->click_count,
                                                   status,
                                                   link_data->sender_device_id,
                                                   link_data->message,
                                                   link_data->id);
          res.push_back(txn);
        }
      }
      return gap_ok;
    });
  return ret;
}

gap_Status
gap_peer_transactions(gap_State* state,
                      std::vector<surface::gap::PeerTransaction>& res)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "transactions",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto const& trs = state.transactions();
      for(auto it = std::begin(trs); it != std::end(trs); ++it)
      {
        auto peer_data =
          std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
            it->second->data());
        if (peer_data != nullptr)
        {
          auto status = state.transactions().at(it->first)->status();
          surface::gap::PeerTransaction txn(
            it->first,
            status,
            state.user_indexes().at(peer_data->sender_id),
            peer_data->sender_device_id,
            state.user_indexes().at(peer_data->recipient_id),
            peer_data->recipient_device_id,
            peer_data->mtime,
            peer_data->files,
            peer_data->total_size,
            peer_data->is_directory,
            peer_data->message,
            peer_data->canceler,
            peer_data->id);
          res.push_back(txn);
        }
      }
      return gap_ok;
    });
}

uint32_t
gap_send_files_by_email(gap_State* state,
                        std::string const& email,
                        std::vector<std::string> const& files,
                        std::string const& message)
{
  ELLE_ASSERT(state != nullptr);
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
               std::vector<std::string> const& files,
               std::string const& message)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
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

gap_Status
gap_pause_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "pause transaction",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.transactions().at(id)->pause();
      return gap_ok;
    });
}

gap_Status
gap_resume_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "resume transaction",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.transactions().at(id)->resume();
      return gap_ok;
    });
}

gap_Status
gap_cancel_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "cancel transaction",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.transactions().at(id)->cancel(true);
      return gap_ok;
    });
}

gap_Status
gap_delete_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "delete link",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.transactions().at(id)->delete_();
      return gap_ok;
    });
}

gap_Status
gap_reject_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "reject transaction",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.transactions().at(id)->reject();
      return gap_ok;
    });
}

gap_Status
gap_accept_transaction(gap_State* state,
                       uint32_t id,
                       boost::optional<std::string const&> relative_output_dir)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "accept transaction",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.transactions().at(id)->accept(relative_output_dir);
      return gap_ok;
    });
}

gap_Status
gap_set_output_dir(gap_State* state,
                   std::string const& output_path,
                   bool fallback)
{
  ELLE_ASSERT(state != nullptr);
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
  ELLE_ASSERT(state != nullptr);
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
  ELLE_ASSERT(state != nullptr);
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
gap_onboarding_set_peer_status(gap_State* state, uint32_t id, bool status)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "change onboarding peer status",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto& tr = state.transactions().at(id);
      if (!dynamic_cast<surface::gap::onboarding::Transaction*>(tr.get()))
        return gap_error;
      // FIXME
      // tr->peer_connection_status(status);
      return gap_ok;
    });
}

/// Change the peer availability status.
gap_Status
gap_onboarding_set_peer_availability(gap_State* state, uint32_t id, bool status)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "change onboarding peer availability",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto& tr = state.transactions().at(id);
      if (!dynamic_cast<surface::gap::onboarding::Transaction*>(tr.get()))
        return gap_error;
      if (status)
        tr->notify_peer_reachable({}, {});
      else
        tr->notify_peer_unreachable();
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
                     std::string const& file,
                     boost::optional<std::vector<std::string>> infinit_files)
{
  ELLE_ASSERT(state != nullptr);
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
                                   user_name,
                                   message,
                                   file,
                                   infinit_files);
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
  ELLE_ASSERT(state != nullptr);
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
