#include <sstream>
#include <stdexcept>

#include <elle/container/map.hh>
#include <elle/container/set.hh>
#include <elle/memory.hh>

#include <reactor/scheduler.hh>

#include <infinit/oracles/trophonius/Client.hh>

#include <surface/gap/State.hh>
#include <surface/gap/Error.hh>
#include <surface/gap/Exception.hh>

ELLE_LOG_COMPONENT("surface.gap.State.User");

namespace surface
{
  namespace gap
  {
    State::UserNotFoundException::UserNotFoundException(uint32_t id):
      Exception{gap_unknown_user, elle::sprintf("unknown user %s", id)}
    {}

    State::UserNotFoundException::UserNotFoundException(std::string const& id):
      Exception{gap_unknown_user, elle::sprintf("unknown user %s", id)}
   {}

    State::NotASwaggerException::NotASwaggerException(uint32_t id):
      Exception{gap_unknown_user, elle::sprintf("unknown swagger %s", id)}
    {}

    State::NotASwaggerException::NotASwaggerException(std::string const& id):
      Exception{gap_unknown_user, elle::sprintf("unknown swagger %s", id)}
    {}

    Notification::Type State::AvatarAvailableNotification::type =
      NotificationType_AvatarAvailable;
    Notification::Type State::UserStatusNotification::type =
      NotificationType_UserStatusUpdate;
    Notification::Type State::NewSwaggerNotification::type =
      NotificationType_NewSwagger;
    Notification::Type State::DeletedSwaggerNotification::type =
      NotificationType_DeletedSwagger;
    Notification::Type State::DeletedFavoriteNotification::type =
      NotificationType_DeletedFavorite;

    State::UserStatusNotification::UserStatusNotification(uint32_t id,
                                                          bool status):
      id(id),
      status(status)
    {}

    State::NewSwaggerNotification::NewSwaggerNotification(uint32_t id):
      id(id)
    {}

    State::DeletedSwaggerNotification::DeletedSwaggerNotification(uint32_t id):
      id(id)
    {}

    State::DeletedFavoriteNotification::DeletedFavoriteNotification(uint32_t id):
      id(id)
    {}

    State::AvatarAvailableNotification::AvatarAvailableNotification(uint32_t id):
      id(id)
    {}

    /// Generate a id for local user.
    static
    uint32_t
    generate_id()
    {
      static uint32_t id = null_id;
      return ++id;
    }

    State::User const&
    State::user_sync(State::User const& user) const
    {
      ELLE_DEBUG_SCOPE("%s: user response: %s", *this, user);

      uint32_t id = 0;
      if (this->_user_indexes.find(user.id) == this->_user_indexes.end())
      {
        id = generate_id();
        this->_user_indexes[user.id] = id;
        this->_users.emplace(id, std::move(user));
      }
      else
      {
        // If the user already in the cache, we keep his index and replace the
        // data by the new fetched one.
        id = this->_user_indexes.at(user.id);
        ELLE_ASSERT_NEQ(id, 0u);
        this->_users.at(id) = std::move(user);
      }

      ELLE_ASSERT_NEQ(id, 0u);
      return this->_users.at(id);
    }

    State::User const&
    State::user_sync(std::string const& id) const
    {
      ELLE_DEBUG_SCOPE("%s: sync user from object id or email: %s", *this, id);
      try
      {
        return this->user_sync(this->meta().user(id));
      }
      catch (infinit::state::UserNotFoundError const& e)
      {
        throw State::UserNotFoundException(id);
      }
    }

    State::User const&
    State::user(std::string const& user_id,
                bool merge) const
    {
      ELLE_DEBUG_SCOPE("%s: user from object id %s", *this, user_id);

      if (this->_user_indexes.find(user_id) == this->_user_indexes.end())
      {
        if (merge)
        {
          ELLE_DEBUG("%s: user not found, merging it", *this);
          return this->user_sync(user_id);
        }

        ELLE_DEBUG("%s: user %s has not been found", *this, user_id)
          for (auto const& user: this->users())
            ELLE_DEBUG("-- %s: %s", user.first, user.second);

        throw State::UserNotFoundException(user_id);
      }

      uint32_t id = this->_user_indexes.at(user_id);
      return this->user(id);
    }

    State::User const&
    State::user(uint32_t id) const
    {
      ELLE_DUMP_SCOPE("%s: get user from id %s", *this, id);

      if (this->_users.find(id) == this->_users.end())
      {
        ELLE_DEBUG("%s: user %s has not been found", *this, id)
          for (auto const& user: this->users())
            ELLE_DEBUG("-- %s: %s", user.first, user.second);

        throw State::UserNotFoundException(id);
      }
      return this->_users.at(id);
    }

    State::User const&
    State::user(std::function<bool (State::UserPair const&)> const& func) const
    {
      ELLE_TRACE_SCOPE("%s: find user", *this);

      typedef UserMap::const_iterator It;

      It begin = this->_users.begin();
      It end = this->_users.end();

      It res = std::find_if(begin, end, func);

      if (res != end)
        return res->second;
      else
      {
        ELLE_DEBUG("%s: user has not been found (from find)", *this)
          for (auto const& user: this->users())
            ELLE_DEBUG("-- %s: %s", user.first, user.second);

        throw State::UserNotFoundException("from find");
      }
    }

    State::User const&
    State::user_from_handle(std::string const& handle) const
    {
      ELLE_TRACE_SCOPE("%s: user from handle %s", *this, handle);
      try
      {
        return this->user(
          [this, handle] (State::UserPair const& pair)
          {
            return pair.second.handle == handle;
          });
      }
      catch (State::UserNotFoundException const&)
      {
        try
        {
          return this->user_sync(this->meta().user_from_handle(handle));
        }
        catch (infinit::state::UserNotFoundError const& e)
        {
          throw State::UserNotFoundException(handle);
        }
      }
    }

    /// Return 2 vectors:
    /// - One containing the newly connected devices.
    /// - One containing the disconnected devices.
    /// The input vectors will be sorted.
    template<typename T>
    static
    std::pair< std::vector<T>, std::vector<T> >
    compare(std::vector<T>& old_user_devices,
            std::vector<T>& new_user_devices)
    {
      std::sort(std::begin(old_user_devices), std::end(old_user_devices));
      std::sort(std::begin(new_user_devices), std::end(new_user_devices));

      std::vector<T> plus(new_user_devices.size());
      std::vector<T> minus(old_user_devices.size());

      {
        auto it = std::set_difference(
          std::begin(old_user_devices),
          std::end(old_user_devices),
          std::begin(new_user_devices),
          std::end(new_user_devices),
          std::begin(minus));

        minus.resize(it - minus.begin());
      }

      {
        auto it = std::set_difference(
          std::begin(new_user_devices),
          std::end(new_user_devices),
          std::begin(old_user_devices),
          std::end(old_user_devices),
          std::begin(plus));

        plus.resize(it - plus.begin());
      }

      return {std::move(plus), std::move(minus)};
    }

    void
    State::_user_resync(std::vector<User> const& users)
    {
      using namespace infinit::oracles;
      ELLE_TRACE_SCOPE("%s: resync user", *this);
      this->_swagger_indexes.clear();
      for (auto const& swagger: users)
      {
        bool init = false;
        if (this->_user_indexes.find(swagger.id) == this->_user_indexes.end())
        {
          auto user = this->user_sync(swagger);
          init = true;
        }

        // Compare the cached user with the remote one, and calculate the
        // diff.
        auto old_user = this->user(swagger.id);
        auto user = this->user_sync(swagger);
        ELLE_DEBUG("old: %s (on devices %s)",
                   old_user, old_user.connected_devices);
        ELLE_DEBUG("new: %s (on devices %s)", user, user.connected_devices);
        // Remove duplicates.
        old_user.connected_devices.erase(
          std::unique(old_user.connected_devices.begin(),
                      old_user.connected_devices.end()),
          old_user.connected_devices.end());
        user.connected_devices.erase(
          std::unique(user.connected_devices.begin(),
                      user.connected_devices.end()),
          user.connected_devices.end());
        auto res = compare<std::string>(
          old_user.connected_devices, user.connected_devices);

        ELLE_DEBUG("%s: %s newly connected device(s): %s",
                   this, res.first.size(), res.first)
          for (auto const& device: init ? swagger.connected_devices : res.first)
          {
            ELLE_DEBUG("%s: updating device %s", *this, device);
            std::unique_ptr<trophonius::UserStatusNotification> notif{new
                trophonius::UserStatusNotification};
            notif->user_id = user.id;
            notif->user_status = user.online();
            notif->device_id = device;
            notif->device_status = true;
            this->handle_notification(std::move(notif));
          }

        ELLE_DEBUG("%s: %s disconnected device(s): %s",
                   this, res.second.size(), res.second)
          for (auto const& device: res.second)
          {
            ELLE_DEBUG("%s: updating device %s", *this, device);
            std::unique_ptr<trophonius::UserStatusNotification> notif{new
                trophonius::UserStatusNotification};
            notif->user_id = user.id;
            notif->user_status = user.online();
            notif->device_id = device;
            notif->device_status = false;
            this->handle_notification(std::move(notif));
          }
        this->_swagger_indexes.insert(this->_user_indexes.at(user.id));
      }

      for (std::string const& user_id: this->me().favorites)
        this->user(user_id);
    }

    std::vector<uint32_t>
    State::users_search(std::string const& text) const
    {
      ELLE_TRACE_METHOD(text);
      auto users = this->meta().users_search(text);
      std::vector<uint32_t> res;
      for (auto const& user: users)
      {
        this->user_sync(user);
        res.push_back(this->_user_indexes.at(user.id));
      }
      return res;
    }

    std::unordered_map<std::string, uint32_t>
    State::users_by_emails(std::vector<std::string> const& emails) const
    {
      auto users = this->meta().search_users_by_emails(emails);
      std::unordered_map<std::string, uint32_t> res;
      for (auto const& user: users)
      {
        this->user_sync(user.second);
        std::pair<std::string, uint32_t> item;
        item.first = user.first;
        item.second = this->_user_indexes.at(user.second.id);
        res.insert(item);
      }
      return res;
    }

    void
    State::_queue_user_icon(std::string const& user_id) const
    {
      auto id = this->_user_indexes.at(this->user(user_id).id);
      if (this->_avatars.find(id) == this->_avatars.end() &&
          this->_avatar_to_fetch.find(user_id) == this->_avatar_to_fetch.end())
      {
        this->_avatar_to_fetch.insert(user_id);
        this->_avatar_fetching_barrier.open();
      }
    }

    elle::ConstWeakBuffer
    State::user_icon(std::string const& user_id) const
    {
      auto id = this->_user_indexes.at(this->user(user_id).id);

      if (this->_avatars.find(id) != this->_avatars.end())
      {
        if (this->_avatar_to_fetch.find(user_id) != this->_avatar_to_fetch.end())
        {
          ELLE_WARN("%s: remove %s avatar from fetching: already fetched",
                    *this, user_id);
          this->_avatar_to_fetch.erase(user_id);
        }
        return this->_avatars[id];
      }

      if (this->_avatar_to_fetch.find(user_id) != this->_avatar_to_fetch.end())
      {
        ELLE_DEBUG("avatar is being fetched");
        throw Exception(gap_data_not_fetched_yet,
                        "avatar is not available yet");
      }

      this->_avatar_to_fetch.insert(user_id);
      this->_avatar_fetching_barrier.open();
      throw Exception(gap_data_not_fetched_yet, "avatar is not available yet");
    }

    void
    State::user_icon_refresh(uint32_t user_id) const
    {
      this->_avatars.erase(user_id);
      this->user_icon(this->users().at(user_id).id);
    }

    bool
    State::device_status(std::string const& user_id,
                         std::string const& device_id) const
    {
      ELLE_TRACE_METHOD(user_id, device_id);

      State::User user = this->user(user_id);

      bool status = std::find(user.connected_devices.begin(),
                              user.connected_devices.end(),
                              device_id) != user.connected_devices.end();

      ELLE_DEBUG("user (%s) device's (%s) is %s",
                 user_id, device_id, (status ? "up" : "down"));

      return status;
    }

    ///- Swaggers --------------------------------------------------------------
    State::UserIndexes
    State::swaggers()
    {
      ELLE_TRACE_SCOPE("%s: get swagger list", *this);

      return this->_swagger_indexes;
    }

    State::User
    State::swagger(std::string const& user_id)
    {
      ELLE_TRACE_SCOPE("%s: swagger from objectid %s", *this, user_id);

      uint32_t id = 0;
      try
      {
        id = this->_user_indexes.at(user_id);
      }
      catch (std::out_of_range const&)
      {
        this->user(user_id);
        id = this->_user_indexes.at(this->user(user_id).id);
      }
      ELLE_ASSERT_NEQ(id, 0u);

      return this->swagger(id);
    }

    State::User
    State::swagger(uint32_t id)
    {
      ELLE_TRACE_SCOPE("%s: swagger from id %s", *this, id);

      if (this->swaggers().find(id) == this->swaggers().end())
        throw NotASwaggerException{id};

      return this->user(id);
    }

    void
    State::_on_new_swagger(
      infinit::oracles::trophonius::NewSwaggerNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: new swagger notification %s", *this, notif);
      uint32_t id = this->_user_indexes.at(this->user_sync(notif.user_id).id);
      {
        reactor::Lock lock(this->_swagger_mutex);
        this->_swagger_indexes.insert(id);
      }
      this->enqueue<NewSwaggerNotification>(NewSwaggerNotification(id));
    }

    void
    State::_on_deleted_swagger(
      infinit::oracles::trophonius::DeletedSwaggerNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: deleted swagger notification %s", *this, notif);
      uint32_t id = this->_user_indexes.at(this->user_sync(notif.user_id).id);
      {
        reactor::Lock lock(this->_swagger_mutex);
        this->_swagger_indexes.erase(id);
      }
      this->enqueue<DeletedSwaggerNotification>(DeletedSwaggerNotification(id));
    }

    void
    State::_on_deleted_favorite(
      infinit::oracles::trophonius::DeletedFavoriteNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: deleted favorite notification %s", *this, notif);
      uint32_t id = this->_user_indexes.at(this->user_sync(notif.user_id).id);
      this->_me->favorites.remove(notif.user_id);
      this->enqueue<DeletedFavoriteNotification>(DeletedFavoriteNotification(id));
    }

    void
    State::_on_swagger_status_update(
      infinit::oracles::trophonius::UserStatusNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: user status notification %s", *this, notif);
      State::User swagger = this->user(notif.user_id);
      if (swagger.ghost() && notif.user_status == gap_user_status_online)
      {
        swagger = this->user_sync(notif.user_id);
        ELLE_ASSERT(!swagger.ghost());
      }
      {
        reactor::Lock lock(this->_swagger_mutex);
        this->_swagger_indexes.insert(this->_user_indexes.at(swagger.id));
      }
      ELLE_DEBUG("%s's (id: %s) status changed to %s",
                 swagger.fullname, swagger.id, notif.user_status);
      ELLE_ASSERT(notif.user_status == gap_user_status_online ||
                  notif.user_status == gap_user_status_offline);
      State::User& user = this->_users.at(this->_user_indexes.at(swagger.id));
      ELLE_DEBUG("%s: device %s status is %s",
                 *this, notif.device_id, notif.device_status);
      if (notif.device_status)
      {
        auto it = std::find(std::begin(user.connected_devices),
                            std::end(user.connected_devices),
                            notif.device_id);
        if (it == std::end(user.connected_devices))
        {
          ELLE_DEBUG("add device to connected device list");
          user.connected_devices.push_back(notif.device_id);
        }
        else
        {
          ELLE_DEBUG("%s: device %s was already in connected devices",
                     *this, notif.device_id);
        }
      }
      else
      {
        auto it = std::find(std::begin(user.connected_devices),
                            std::end(user.connected_devices),
                            notif.device_id);
        if (it != std::end(user.connected_devices))
        {
          ELLE_DEBUG("remove device from connected device list");
          user.connected_devices.erase(it);
        }
        else
        {
          ELLE_DEBUG("%s: device %s status wasn't in connected_devices",
                     *this, notif.device_id);
        }
      }
      ELLE_DUMP("%s connected devices: %s", user, user.connected_devices);
      ELLE_DEBUG("signal connection status update to concerned transactions")
        for (auto& transaction: this->transactions())
        {
          transaction.second->notify_user_connection_status(
            notif.user_id, notif.user_status,
            notif.device_id, notif.device_status);
        }
      ELLE_DEBUG("enqueue notification for UI")
        this->enqueue<UserStatusNotification>(
          UserStatusNotification(
            this->_user_indexes.at(swagger.id), notif.user_status));
    }
  }
}
