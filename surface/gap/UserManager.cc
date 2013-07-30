#include "UserManager.hh"

#include <elle/memory.hh>
#include <elle/os/path.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <common/common.hh>

#include <lune/Dictionary.hh>
#include <papier/Identity.hh>

#include <boost/filesystem.hpp>

#include <surface/gap/gap.h> // for user_status_online...

ELLE_LOG_COMPONENT("infinit.surface.gap.User");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;
    namespace path = elle::os::path;

    UserManager::UserManager(NotificationManager& notification_manager,
                             plasma::meta::Client& meta,
                             SelfGetter const& self):
      Notifiable{notification_manager},
      _meta(meta),
      _self{self},
      _swaggers_dirty(true)
    {
      ELLE_TRACE_METHOD("");

      using std::placeholders::_1;
      this->_notification_manager.user_status_callback(
        std::bind(&UserManager::_on_swagger_status_update, this, _1));
      this->_notification_manager.new_swagger_callback(
        std::bind(&UserManager::_on_new_swagger, this, _1));
      this->_notification_manager.add_resync_callback(
        std::bind(&UserManager::_on_resync, this));
    }

    UserManager::~UserManager()
    {
      ELLE_TRACE_METHOD("");
    }

    User
    UserManager::_sync(plasma::meta::UserResponse const& response)
    {
      ELLE_TRACE_SCOPE("%s: user response: %s", *this, response);

      auto user = elle::make_unique<User>(
        response.id,
        response.fullname,
        response.handle,
        response.public_key,
        response.status,
        response.connected_devices);
      this->_users(
        [&] (UserMap& users) { users[response.id].reset(user.get()); });
      return *user.release();
    }

    User
    UserManager::_sync(std::string const& id)
    {
      ELLE_DEBUG_METHOD(id);

      return this->_sync(this->_meta.user(id));
    }

    User
    UserManager::one(std::string const& id)
    {
      ELLE_DEBUG_METHOD(id);
      auto user = this->_users([&] (UserMap& users) {
          auto it = users.find(id);
          if (it != users.end())
            return *(it->second);
          return User();
        });
      if (not user.id.empty())
        return user;
      return this->_sync(id);
    }

    User
    UserManager::from_public_key(std::string const& public_key)
    {
      ELLE_DEBUG_METHOD(public_key);

      auto id = this->_users(
        [this, public_key] (UserMap& users) -> std::string {
          for (auto const& pair : users)
            if (pair.second->public_key == public_key)
              return pair.first;
          return "";
        });
      if (not id.empty())
        return this->one(id);
      return this->_sync(this->_meta.user_from_public_key(public_key));
    }

    void
    UserManager::_on_resync()
    {
      this->_swaggers_dirty = true;
      auto swaggers = this->swaggers();

      // Skeleton of user notif (we do not care abot device status).
      UserStatusNotification n;
      n.notification_type = NotificationType::user_status;
      n.device_id = "";
      n.device_status = false;
      for (auto const& swagger_id: swaggers)
      {
        auto old_user = this->one(swagger_id);
        auto user = this->_meta.user(swagger_id);
        bool need_resync = this->_users([this, &user] (UserMap& users)-> bool {
            auto it = users.find(user.id);
            return (
              it == users.end() || it->second->status != user.status);
          });

        if (old_user.connected_devices.size() != user.connected_devices.size())
          need_resync = true;
        else
        {
          old_user.connected_devices.sort();
          user.connected_devices.sort();
          std::list<std::string> devices;
          std::set_difference(
            old_user.connected_devices.begin(),
            old_user.connected_devices.end(),
            user.connected_devices.begin(),
            user.connected_devices.end(),
            devices.begin());
          need_resync = not devices.empty();
        }
        this->_sync(user);
        if (need_resync)
        {
          n.user_id = user.id;
          n.status = user.status;
          this->_notification_manager.fire_callbacks(n, false);
        }
      }
    }

    std::map<std::string, User>
    UserManager::search(std::string const& text)
    {
      ELLE_TRACE_METHOD(text);

      std::map<std::string, User> result;
      auto res = this->_meta.search_users(text);
      for (auto const& user_id : res.users)
        result[user_id] = this->one(user_id);
      return result;
    }

    bool
    UserManager::device_status(std::string const& user_id,
                               std::string const& device_id)
    {
      ELLE_TRACE_METHOD(user_id, device_id);

      auto user = this->one(user_id);
      bool status = std::find(
        user.connected_devices.begin(),
        user.connected_devices.end(),
        device_id) != user.connected_devices.end();

      ELLE_DEBUG("user (%s) device's (%s) is %s",
                 user_id, device_id, (status ? "up" : "down"));

      return status;
    }

    elle::Buffer
    UserManager::icon(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

      return this->_meta.user_icon(id);
    }

    void
    UserManager::send_message(std::string const& recipient_id,
                              std::string const& message)
    {
      ELLE_TRACE_METHOD(recipient_id, message);

      this->_meta.send_message(recipient_id, this->_self().id, message);
    }

    ///- Swaggers --------------------------------------------------------------
    UserManager::SwaggerSet
    UserManager::swaggers()
    {
      ELLE_TRACE_METHOD("");

      if (this->_swaggers_dirty)
      {
        auto response = this->_meta.get_swaggers();

        this->_swaggers(
          [&] (SwaggerSet& swaggers) -> SwaggerSet {
            swaggers.clear();
            for (auto const& swagger_id: response.swaggers)
              swaggers.insert(swagger_id);
            return swaggers;
          });
        this->_swaggers_dirty = false;
      }
      return this->_swaggers(
        [this] (SwaggerSet& swagger) -> SwaggerSet {
          return swagger;
        });
    }

    User
    UserManager::swagger(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);
      this->_swaggers([this, id] (SwaggerSet& swaggers) {
        if (swaggers.find(id) == swaggers.end())
          throw Exception{
            gap_error,
            "Cannot find any swagger for id '" + id + "'"};

        });
      return this->one(id);
    }

    void
    UserManager::_on_new_swagger(NewSwaggerNotification const& notification)
    {
      this->one(notification.user_id);
      this->_swaggers->insert(notification.user_id);
    }

    void
    UserManager::_on_swagger_status_update(UserStatusNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: received user status notification %s", *this, notif);

      auto swagger = this->one(notif.user_id);
      if (swagger.public_key.empty() && notif.status == gap_user_status_online)
      {
        swagger = this->_sync(notif.user_id);
        ELLE_ASSERT(not swagger.public_key.empty());
      }
      this->swaggers(); // force up-to-date swaggers
      this->_swaggers->insert(swagger.id);
      ELLE_DEBUG("%s's (id: %s) status changed to %s",
                 swagger.fullname, swagger.id, notif.status);
      ELLE_ASSERT(notif.status == gap_user_status_online ||
                  notif.status == gap_user_status_offline);

      // Update user status.
      this->_users([&] (UserMap& map) {
        auto it = map.find(notif.user_id);
        ELLE_ASSERT(it != map.end());
        ELLE_ASSERT(it->second != nullptr);
        it->second->status = notif.status;

        // Update connected devices.
        if (notif.device_id.size() > 0)
        {
          std::remove(
            it->second->connected_devices.begin(),
            it->second->connected_devices.end(),
            notif.device_id);
          if (notif.device_status)
            it->second->connected_devices.push_back(notif.device_id);
        }
      });
    }

    void
    UserManager::swaggers_dirty()
    {
      ELLE_TRACE_METHOD("");

      this->_swaggers_dirty = true;
    }

    /*----------.
    | Printable |
    `----------*/
    void
    UserManager::print(std::ostream& stream) const
    {
      stream << "NetworkManager(" << this->_meta.email() << ")";
    }
  }
}
