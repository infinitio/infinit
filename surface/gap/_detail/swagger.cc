#include "../State.hh"

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    State::SwaggersMap const&
    State::swaggers()
    {
      if (this->_swaggers_dirty)
        {
          auto response = this->_meta->get_swaggers();
          for (auto const& swagger_id: response.swaggers)
            {
              if (this->_swaggers.find(swagger_id) == this->_swaggers.end())
                {
                  auto response = this->_meta->user(swagger_id);
                  this->_swaggers[swagger_id] = new User{
                      response._id,
                      response.fullname,
                      response.handle,
                      response.public_key,
                      response.status,
                  };
                }
            }
          this->_swaggers_dirty = false;
        }
      return this->_swaggers;
    }

    User const&
    State::swagger(std::string const& id)
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
    State::_on_user_status_update(UserStatusNotification const& notif)
    {
      ELLE_ASSERT(
        this->swaggers().find(notif.user_id) != this->swaggers().end()
      );

      auto it = this->swagger(notif.user_id);
      ELLE_DEBUG("%s's status changed to %s", it.fullname, notif.status);
      ELLE_ASSERT(
            notif.status == gap_user_status_online
        or  notif.status == gap_user_status_offline
      );
      it.status = notif.status;
    }

  }
}
