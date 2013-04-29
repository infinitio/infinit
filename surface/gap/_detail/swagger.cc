#include "../State.hh"

ELLE_LOG_COMPONENT("infinit.surface.gap.State");

namespace surface
{
  namespace gap
  {

    State::SwaggersSet const&
    State::swaggers()
    {
      if (this->_swaggers_dirty)
        {
          auto response = this->_meta->get_swaggers();
          this->_swaggers.clear();
          for (auto const& swagger_id: response.swaggers)
            this->_swaggers.insert(swagger_id);
          this->_swaggers_dirty = false;
        }
      return this->_swaggers;
    }

    User const&
    State::swagger(std::string const& id)
    {
      auto it = this->swaggers().find(id);
      if (it == this->swaggers().end())
        throw Exception{
            gap_error,
            "Cannot find any swagger for id '" + id + "'"
        };
      return this->user(id);
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
