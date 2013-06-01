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
      this->_notification_manager.user_status_callback(
        [&] (UserStatusNotification const &n) -> void
        {
          this->_on_swagger_status_update(n);
        }
      );

      ELLE_TRACE_METHOD("");
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
      std::unique_ptr<User> user{
        new User{
          response.id,
          response.fullname,
          response.handle,
          response.public_key,
          response.status,
          response.connected_devices,
        }};

      this->_users[response.id] = user.get();
      for (auto const& dev: user->connected_devices)
        this->_connected_devices.insert(dev);
      return *(user.release());
    }

    User const&
    UserManager::from_public_key(std::string const& public_key)
    {
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
      std::map<std::string, User const*> result;
      auto res = this->_meta.search_users(text);
      for (auto const& user_id : res.users)
     {
       result[user_id] = &this->one(user_id);
     }
      return result;
    }

    bool
    UserManager::device_status(std::string const& device_id) const
    {
      return (this->_connected_devices.find(device_id) !=
              this->_connected_devices.end());
    }

    elle::Buffer
    UserManager::icon(std::string const& id)
    {
      return this->_meta.user_icon(id);
    }

    std::string
    UserManager::invite(std::string const& email)
    {
      auto response = this->_meta.invite_user(email);
      return response._id;
    }

    void
    UserManager::send_message(std::string const& recipient_id,
                        std::string const& message)
    {
      this->_meta.send_message(recipient_id,
                               this->_self.id,
                               message);
    }

    ///- Swaggers --------------------------------------------------------------
    UserManager::SwaggersMap const&
    UserManager::swaggers()
    {
      if (this->_swaggers_dirty)
      {
        auto response = this->_meta.get_swaggers();
        for (auto const& swagger_id: response.swaggers)
        {
          if (this->_swaggers.find(swagger_id) == this->_swaggers.end())
          {
            auto response = this->_meta.user(swagger_id);
            this->_swaggers[swagger_id] = new User{response.id,
                                                   response.fullname,
                                                   response.handle,
                                                   response.public_key,
                                                   response.status, };
          }
        }
        this->_swaggers_dirty = false;
      }
      return this->_swaggers;
    }

    User const&
    UserManager::swagger(std::string const& id)
    {
      auto it = this->swaggers().find(id);
      if (it == this->swaggers() .end())
        throw Exception{
            gap_error,
            "Cannot find any swagger for id '" + id + "'"
        };
      return *(it->second);
    }

    void
    UserManager::_on_swagger_status_update(UserStatusNotification const& notif)
    {
      ELLE_ASSERT(
        this->swaggers().find(notif.user_id) != this->swaggers().end()
      );

      auto it = this->swagger(notif.user_id);
      ELLE_DEBUG("%s's status changed to %s", it.fullname, notif.status);
      ELLE_ASSERT(notif.status == gap_user_status_online ||
                  notif.status == gap_user_status_offline);

      it.status = notif.status;
      if (notif.device_status)
        this->_connected_devices.insert(notif.device_id);
      else
        this->_connected_devices.erase(notif.device_id);
    }

   void
   UserManager::swaggers_dirty()
   {
     this->_swaggers_dirty = true;
   }

  }
}
