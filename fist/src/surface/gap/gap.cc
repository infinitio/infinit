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
gap_new(bool production, std::string const& download_dir)
{
  try
  {
    gap_State* state = new gap_State(production, download_dir);
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
          std::string const& password)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "login",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.login(email, password);
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
             std::string const& password)
{
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
    state,
    "register",
    [&] (surface::gap::State& state) -> gap_Status
    {
     state.register_(fullname, email, password);
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
  ELLE_ASSERT(state != nullptr);
  return run<gap_Status>(
  state,
  "set self email",
  [&] (surface::gap::State& state) -> gap_Status
  {
    auto hashed_password =
      state.hash_password(state.me().email, password);
    state.meta().change_email(email, hashed_password);
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
      state.change_password(
        state.hash_password(state.me().email, old_password),
        state.hash_password(state.me().email, new_password));
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

std::vector<uint32_t>
gap_self_favorites(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<std::vector<uint32_t>>(
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

surface::gap::User
gap_user_by_id(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<surface::gap::User>(
    state,
    "user by id",
    [&] (surface::gap::State& state) -> surface::gap::User
    {
      auto const& user = state.user(id);
      surface::gap::User res(
        state.user_indexes().at(user.id),
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        user.deleted(),
        user.ghost());
      return res;
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

void
gap_refresh_avatar(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  run<gap_Status>(
    state,
    "refresh user avatar",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.user_icon_refresh(id);
      return gap_ok;
    });
}

surface::gap::User
gap_user_by_email(gap_State* state, std::string const& email)
{
  ELLE_ASSERT(state != nullptr);
  return run<surface::gap::User>(
    state,
    "user by email",
    [&] (surface::gap::State& state) -> surface::gap::User
    {
      auto user = state.user(email, true);
      surface::gap::User res(
        state.user_indexes().at(user.id),
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        user.deleted(),
        user.ghost());
      return res;
    });
}

surface::gap::User
gap_user_by_handle(gap_State* state, std::string const& handle)
{
  ELLE_ASSERT(state != nullptr);
  return run<surface::gap::User>(
    state,
    "user by handle",
    [&] (surface::gap::State& state) -> surface::gap::User
    {
      auto user = state.user_from_handle(handle);
      surface::gap::User res(
        state.user_indexes().at(user.id),
        user.online(),
        user.fullname,
        user.handle,
        user.id,
        user.deleted(),
        user.ghost());
      return res;
    });
}

std::vector<surface::gap::User>
gap_users_search(gap_State* state, std::string const& text)
{
  ELLE_ASSERT(state != nullptr);
  return run<std::vector<surface::gap::User>>(
    state,
    "users search",
    [&] (surface::gap::State& state) -> std::vector<surface::gap::User>
    {
      return state.users_search(text);
    });
}

std::unordered_map<std::string, surface::gap::User>
gap_users_by_emails(gap_State* state, std::vector<std::string> emails)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(emails.size() != 0);
  return run<std::unordered_map<std::string, surface::gap::User>>(
    state,
    "emails and users",
    [&] (surface::gap::State& state) -> std::unordered_map<std::string, surface::gap::User>
    {
      return state.users_by_emails(emails);
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

std::vector<surface::gap::User>
gap_swaggers(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<std::vector<surface::gap::User>>(
    state,
    "swaggers",
    [&] (surface::gap::State& state) -> std::vector<surface::gap::User>
    {
      std::vector<surface::gap::User> res;
      for (uint32_t user_id: state.swaggers())
      {
        auto user = state.user(user_id);
        surface::gap::User ret_user(
          user_id,
          user.online(),
          user.fullname,
          user.handle,
          user.id,
          user.deleted(),
          user.ghost());
        res.push_back(ret_user);
      }
      return res;
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
      state.meta().favorite(meta_id);
      std::list<std::string>& favorites =
        const_cast<std::list<std::string>&>(state.me().favorites);
      if (std::find(favorites.begin(),
                    favorites.end(),
                    meta_id) == favorites.end())
        favorites.push_back(meta_id);
      if (state.metrics_reporter())
        state.metrics_reporter()->user_favorite(meta_id);
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
      state.meta().unfavorite(meta_id);
      // XXX Should be notification driven
      std::list<std::string>& favorites =
        const_cast<std::list<std::string>&>(state.me().favorites);
      auto it = std::find(favorites.begin(),
                          favorites.end(),
                          meta_id);
      if (it != favorites.end())
        favorites.erase(it);
      if (state.metrics_reporter())
        state.metrics_reporter()->user_unfavorite(meta_id);
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
gap_trophonius_unavailable_callback(
  gap_State* state,
  std::function<void ()> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  auto cb_wrapper =
    [callback]
    (surface::gap::State::TrophoniusUnavailable const& notification)
    {
      callback();
    };
  return run<gap_Status>(
    state,
    "trophonius unavailable callback",
    [&] (surface::gap::State& state) -> gap_Status
    {
      state.attach_callback<
        surface::gap::State::TrophoniusUnavailable>(cb_wrapper);
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
gap_critical_callback(
  gap_State* state,
  std::function<void ()> const& callback)
{
  ELLE_ASSERT(state != nullptr);
  return state->gap_critical_callback(state, callback);
}

surface::gap::PeerTransaction
gap_peer_transaction_by_id(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<surface::gap::PeerTransaction>(
    state,
    "peer transaction",
    [&] (surface::gap::State& state) -> surface::gap::PeerTransaction
    {
      auto peer_data =
        std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
          state.transactions().at(id)->data());
      ELLE_ASSERT(peer_data != nullptr);
      auto status = state.transactions().at(id)->status();
      surface::gap::PeerTransaction res(
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
        peer_data->canceler);
      return res;
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

surface::gap::LinkTransaction
gap_link_transaction_by_id(gap_State* state, uint32_t id)
{
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
                                               status,
                                               data->sender_device_id);
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
                                                   link_data->sender_device_id);
          values.push_back(txn);
        }
      }
      return values;
    });
  return ret;
}

std::vector<surface::gap::PeerTransaction>
gap_peer_transactions(gap_State* state)
{
  ELLE_ASSERT(state != nullptr);
  return run<std::vector<surface::gap::PeerTransaction>>(
    state,
    "transactions",
    [&] (surface::gap::State& state) -> std::vector<surface::gap::PeerTransaction>
    {
      std::vector<surface::gap::PeerTransaction> values;
      auto const& trs = state.transactions();
      for(auto it = std::begin(trs); it != std::end(trs); ++it)
      {
        auto peer_data =
          std::dynamic_pointer_cast<infinit::oracles::PeerTransaction>(
            it->second->data());
        if (peer_data != nullptr)
        {
          auto status = state.transactions().at(it->first)->status();
          surface::gap::PeerTransaction res(
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
            peer_data->canceler);
          values.push_back(res);
        }
      }
      return values;
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

uint32_t
gap_cancel_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
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
  ELLE_ASSERT(id != surface::gap::null_id);
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
gap_reject_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
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
gap_accept_transaction(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<uint32_t>(
    state,
    "accept transaction",
    [&] (surface::gap::State& state) -> uint32_t
    {
      state.transactions().at(id)->accept();
      return id;
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

/// Force transfer deconnection.
gap_Status
gap_onboarding_interrupt_transfer(gap_State* state, uint32_t id)
{
  ELLE_ASSERT(state != nullptr);
  ELLE_ASSERT(id != surface::gap::null_id);
  return run<gap_Status>(
    state,
    "interrupt onboarding transfer",
    [&] (surface::gap::State& state) -> gap_Status
    {
      auto const& tr = state.transactions().at(id);
      if (!dynamic_cast<surface::gap::onboarding::Transaction*>(tr.get()))
        return gap_error;
      tr->interrupt();
      return gap_ok;
    });
}

gap_Status
gap_send_user_report(gap_State* state,
                     std::string const& user_name,
                     std::string const& message,
                     std::string const& file)
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
