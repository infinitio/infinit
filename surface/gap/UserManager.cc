#include "UserManager.hh"

#include <elle/os/path.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <common/common.hh>

#include <lune/Dictionary.hh>
#include <lune/Identity.hh>

#include <boost/filesystem.hpp>

#include <surface/gap/gap.h> // for user_status_online...

ELLE_LOG_COMPONENT("infinit.surface.gap.User");

namespace surface
{
  namespace gap
  {
    namespace fs = boost::filesystem;
    namespace path = elle::os::path;

    UserManager::UserManager(NotifManager& notification_manager,
                             plasma::meta::Client& meta,
                             Self const& self):
      Notifiable{notification_manager},
      _meta(meta),
      _self(self),
      _swaggers_dirty(true)
    {
      ELLE_TRACE_METHOD("");

      this->_notification_manager.user_status_callback(
        [&] (UserStatusNotification const &n) -> void
        {
          this->_on_swagger_status_update(n);
        }
      );
    }

    UserManager::~UserManager()
    {
      ELLE_TRACE_METHOD("");
    }

    User const&
    UserManager::one(std::string const& id)
    {
      auto it = this->_users.find(id);
      if (it != this->_users.end())
      {
        return *(it->second);
      }
      auto response = this->_meta.user(id);
      std::unique_ptr<User> user_ptr{
        new User{
          response.id,
          response.fullname,
          response.handle,
          response.public_key,
          response.status,
          response.connected_devices,
        }};

      auto const& user = *(this->_users[response.id] = user_ptr.get());
      user_ptr.release();

      for (auto const& dev: user.connected_devices)
        this->_connected_devices.insert(dev);
      return user;
    }

    User const&
    UserManager::from_public_key(std::string const& public_key)
    {
      ELLE_TRACE_METHOD(public_key);

      for (auto const& pair : this->_users)
      {
        if (pair.second->public_key == public_key)
          return *(pair.second);
      }
      auto response = this->_meta.user_from_public_key(public_key);
      std::unique_ptr<User> user{new User{
          response.id,
          response.fullname,
          response.handle,
          response.public_key,
          response.status,
      }};

      this->_users[response.id] = user.get();
      return *(user.release());
    }

    std::map<std::string, User const*>
    UserManager::search(std::string const& text)
    {
      ELLE_TRACE_METHOD(text);

      std::map<std::string, User const*> result;
      auto res = this->_meta.search_users(text);
      for (auto const& user_id : res.users)
     {
       result[user_id] = &this->one(user_id);
     }
      return result;
    }

    bool
    UserManager::device_status(std::string const& user_id,
                               std::string const& device_id)
    {
      ELLE_TRACE_METHOD(user_id, device_id);

      this->one(user_id); // Ensure the user is loaded.
      bool status = (this->_connected_devices.find(device_id) !=
                     this->_connected_devices.end());
      ELLE_DEBUG("device %s is %s", device_id, (status ? "up" : "down"));
      return status;
    }

    elle::Buffer
    UserManager::icon(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

      return this->_meta.user_icon(id);
    }

    std::string
    UserManager::invite(std::string const& email)
    {
      ELLE_TRACE_METHOD(email);

      auto response = this->_meta.invite_user(email);
      return response._id;
    }

    void
    UserManager::send_message(std::string const& recipient_id,
                              std::string const& message)
    {
      ELLE_TRACE_METHOD(recipient_id, message);

      this->_meta.send_message(recipient_id,
                               this->_self.id,
                               message);
    }

    ///- Swaggers --------------------------------------------------------------
    UserManager::SwaggersSet const&
    UserManager::swaggers()
    {
      ELLE_TRACE_METHOD("");

      if (this->_swaggers_dirty)
      {
        auto response = this->_meta.get_swaggers();
        this->_swaggers.clear();
        for (auto const& swagger_id: response.swaggers)
          this->_swaggers.insert(swagger_id);

        this->_swaggers_dirty = false;
      }
      return this->_swaggers;
    }

    User const&
    UserManager::swagger(std::string const& id)
    {
      ELLE_TRACE_METHOD(id);

      if (this->swaggers().find(id) == this->swaggers().end())
        throw Exception{
          gap_error,
          "Cannot find any swagger for id '" + id + "'"};
      return this->one(id);
    }

    void
    UserManager::_on_swagger_status_update(UserStatusNotification const& notif)
    {
      ELLE_DEBUG_METHOD(notif);

      auto swagger = this->one(notif.user_id);
      this->swaggers(); // force up-to-date swaggers
      this->_swaggers.insert(swagger.id);
      ELLE_DEBUG("%s's status changed to %s", swagger, notif.status);
      ELLE_ASSERT(notif.status == gap_user_status_online ||
                  notif.status == gap_user_status_offline);

      // Update user status.
      auto it = this->_users.find(notif.user_id);
      ELLE_ASSERT(it != this->_users.end());
      ELLE_ASSERT(it->second != nullptr);
      it->second->status = notif.status;

      // Update connected devices.
      if (notif.device_status)
        this->_connected_devices.insert(notif.device_id);
      else
        this->_connected_devices.erase(notif.device_id);
    }

   void
   UserManager::swaggers_dirty()
   {
     ELLE_TRACE_METHOD("");

     this->_swaggers_dirty = true;
   }

  }
}
