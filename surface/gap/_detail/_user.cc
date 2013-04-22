#include "../State.hh"
#include "../UserManager.hh"

#include <elle/os/path.hh>
#include <metrics/_details/google.hh>
#include <elle/serialize/HexadecimalArchive.hh>

#include <common/common.hh>

#include <lune/Dictionary.hh>
#include <lune/Identity.hh>

#include <boost/filesystem.hpp>


ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {
    using MKey = elle::metrics::Key;
    namespace fs = boost::filesystem;
    namespace path = elle::os::path;


    UserManager::UserManager(plasma::meta::Client& meta):
      meta{meta}
   {}

    User const&
    State::user(std::string const& id)
    {
      auto it = this->_users.find(id);
      if (it != this->_users.end())
      {
        // Search user.
        // metrics::google::server().store("Search-user", {{"cd1", "local"}, {"cd2", id}});

        return *(it->second);
      }
      auto response = this->meta->user(id);
      std::unique_ptr<User> user{new User{
          response._id,
          response.fullname,
          response.handle,
          response.public_key,
          response.status}};

      // metrics::google::server().store("Search-user", {{"cd1", "server"}, {"cd2", id}});

      this->_users[response._id] = user.get();
      return *(user.release());
    }

    User const&
    State::user_from_public_key(std::string const& public_key)
    {
      for (auto const& pair : this->_users)
        {
          if (pair.second->public_key == public_key)
            return *(pair.second);
        }
      auto response = this->meta->user_from_public_key(public_key);
      std::unique_ptr<User> user{new User{
          response._id,
          response.fullname,
          response.handle,
          response.public_key,
          response.status,
      }};

      this->_users[response._id] = user.get();
      return *(user.release());
    }

    std::map<std::string, User const*>
    State::search_users(std::string const& text)
    {
      std::map<std::string, User const*> result;
      auto res = this->meta->search_users(text);
      for (auto const& user_id : res.users)
        {
          result[user_id] = &this->user(user_id);
        }
      return result;
    }

    elle::Buffer
    State::user_icon(std::string const& id)
    {
      return this->meta->user_icon(id);
    }

    std::string
    State::invite_user(std::string const& email)
    {
      auto response = this->meta->invite_user(email);
      return response._id;
    }

    void
    State::send_message(std::string const& recipient_id,
                        std::string const& message)
    {
      this->meta->send_message(recipient_id,
                                this->_me._id,
                                message);
    }

    ///- Swaggers --------------------------------------------------------------
    UserManager::SwaggersMap const&
    UserManager::swaggers()
    {
      if (this->_swaggers_dirty)
      {
        auto response = this->meta->get_swaggers();
        for (auto const& swagger_id: response.swaggers)
        {
          if (this->_swaggers.find(swagger_id) == this->_swaggers.end())
          {
            auto response = this->meta->user(swagger_id);
            this->_swaggers[swagger_id] = new User{response._id,
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
    }


  }
}
