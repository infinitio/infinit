#include <surface/gap/State.hh>

#include <reactor/scheduler.hh>

#include <stdexcept>

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

    Notification::Type State::UserStatusNotification::type =
      NotificationType_UserStatusUpdate;
    Notification::Type State::NewSwaggerNotification::type =
      NotificationType_NewSwagger;

    State::UserStatusNotification::UserStatusNotification(uint32_t id,
                                                          bool status):
      id(id),
      status(status)
    {}

    State::NewSwaggerNotification::NewSwaggerNotification(uint32_t id):
      id(id)
    {}

    /// Generate a id for local user.
    static
    uint32_t
    generate_id()
    {
      static uint32_t id = 0;
      return ++id;
    }

    void
    State::clear_users()
    {
      this->_users.clear();
      this->_user_indexes.clear();
      this->_swagger_indexes.clear();
      this->_swaggers_dirty = true;
    }

    State::User
    State::user_sync(State::User const& user) const
    {
      ELLE_TRACE_SCOPE("%s: user response: %s", *this, user);

      uint32_t id = 0;
      try
      {
        // If the user already in the cache, we keep his index and replace the
        // data by the new fetched one.
        id = this->_user_indexes.at(user.id);
        ELLE_ASSERT_NEQ(id, 0u);
        this->_users.at(id) = std::move(user);
      }
      catch (std::out_of_range const&)
      {
        id = generate_id();
        this->_user_indexes[user.id] = id;
        this->_users.emplace(id, std::move(user));
      }

      ELLE_ASSERT_NEQ(id, 0u);
      return this->_users.at(id);
    }

    State::User
    State::user_sync(std::string const& id) const
    {
      ELLE_TRACE_SCOPE("%s: sync user from object id %s", *this, id);

      return this->user_sync(this->meta().user(id));
    }

    State::User
    State::user(std::string const& user_id,
                bool merge) const
    {
      ELLE_TRACE_SCOPE("%s: user from object id %s", *this, user_id);

      try
      {
        uint32_t id = this->_user_indexes.at(user_id);
        return this->user(id);
      }
      catch (std::out_of_range const&)
      {
        if (merge)
        {
          ELLE_DEBUG("%s: user not found, merging it", *this);
          return this->user_sync(user_id);
        }

        throw State::UserNotFoundException(user_id);
      }
    }

    State::User
    State::user(uint32_t id) const
    {
      ELLE_TRACE_SCOPE("%s: sync user from id %s", *this, id);

      try
      {
        return this->_users.at(id);
      }
      catch (std::out_of_range const&)
      {
        throw State::UserNotFoundException(id);
      }
    }

    State::User
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
        throw State::UserNotFoundException("from find");
    }

    State::User
    State::user_from_public_key(std::string const& public_key) const
    {
      ELLE_TRACE_SCOPE("%s: user from public key %s", *this, public_key);
      try
      {
        return this->user([this, public_key] (State::UserPair const& pair)
                          {
                            return pair.second.public_key == public_key;
                          });
      }
      catch (State::UserNotFoundException const&)
      {
        return this->user_sync(this->meta().user_from_public_key(public_key));
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

      std::vector<T> plus(old_user_devices.size());
      std::vector<T> minus(new_user_devices.size());

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
    State::_user_on_resync()
    {
      this->_swaggers_dirty = true;

      for (auto const& swagger_id: this->swaggers())
      {
        // Compare the cached user with the remote one, and calculate the
        // diff.
        auto old_user = this->user(swagger_id);
        auto user = this->user_sync(old_user.id);

        auto res = compare<std::string>(old_user.connected_devices,
                                        user.connected_devices);

        ELLE_DEBUG("%s: %s newly connected device(s)", this, res.first.size())
          for (auto const& device: res.first)
          {
            ELLE_DEBUG("%s: updating device %s", *this, device);
            auto* notif_ptr = new plasma::trophonius::UserStatusNotification{};
            notif_ptr->user_id = swagger_id;
            notif_ptr->status = user.status();
            notif_ptr->device_id = device;
            notif_ptr->device_status = true;

            std::unique_ptr<plasma::trophonius::UserStatusNotification>
              notif(notif_ptr);

            this->handle_notification(std::move(notif));
          }

        ELLE_DEBUG("%s: %s disconnected device(s)", this, res.first.size())
          for (auto const& device: res.first)
          {
            ELLE_DEBUG("%s: updating device %s", *this, device);
            auto* notif_ptr = new plasma::trophonius::UserStatusNotification{};
            notif_ptr->user_id = swagger_id;
            notif_ptr->status = user.status();
            notif_ptr->device_id = device;
            notif_ptr->device_status = false;

            std::unique_ptr<plasma::trophonius::UserStatusNotification>
              notif(notif_ptr);

            this->handle_notification(std::move(notif));
          }
      }
    }

    State::UserIndexes
    State::user_search(std::string const& text) const
    {
      ELLE_TRACE_METHOD(text);

      State::UserIndexes result;
      auto res = this->meta().search_users(text);
      for (auto const& user_id : res.users)
      {
        result.emplace(this->_user_indexes.at(this->user(user_id).id));
      }
      return result;
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

    elle::Buffer
    State::icon(uint32_t id)
    {
      ELLE_TRACE_METHOD(id);

      return this->meta().user_icon(this->user(id).id);
    }

    std::string
    State::invite(std::string const& email)
    {
      ELLE_TRACE_METHOD(email);

      auto response = this->meta().invite_user(email);
      return response._id;
    }

    ///- Swaggers --------------------------------------------------------------
    State::UserIndexes
    State::swaggers()
    {
      ELLE_TRACE_SCOPE("%s: get swagger list", *this);

      // XXX
      // If one Thread is lock on scheded swaggers request, and an other is
      // querying for swaggers, swaggers_dirty will be wrong. So mutex sounds
      // good here.
      ELLE_ASSERT(reactor::Scheduler::scheduler() != nullptr);

      reactor::Lock lock(this->_swagger_mutex);

      if (this->_swaggers_dirty)
      {
        ELLE_DEBUG("%s: swaggers were dirty", *this);
        this->_swagger_indexes.clear();
        for (auto const& user_id: this->meta().get_swaggers().swaggers)
        {
          ELLE_DEBUG("%s: get swagger %s", *this, user_id);
          this->_swagger_indexes.insert(
            this->_user_indexes.at(this->user(user_id).id));
        }

        this->_swaggers_dirty = false;
      }

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
      plasma::trophonius::NewSwaggerNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: new swagger notification %s", *this, notif);
      uint32_t id = this->_user_indexes.at(this->user(notif.user_id).id);
      {
        reactor::Lock lock(this->_swagger_mutex);
        this->_swagger_indexes.insert(id);
      }
      this->enqueue<NewSwaggerNotification>(NewSwaggerNotification(id));
    }

    void
    State::_on_swagger_status_update(
      plasma::trophonius::UserStatusNotification const& notif)
    {
      ELLE_TRACE_SCOPE("%s: user status notification %s", *this, notif);

      State::User swagger = this->user(notif.user_id);
      if (swagger.public_key.empty() && notif.status == gap_user_status_online)
      {
        swagger = this->user_sync(notif.user_id);
        ELLE_ASSERT(not swagger.public_key.empty());
      }
      this->swaggers(); // force up-to-date swaggers

      {
        reactor::Lock lock(this->_swagger_mutex);
        this->_swagger_indexes.insert(this->_user_indexes.at(swagger.id));
      }

      ELLE_DEBUG("%s's (id: %s) status changed to %s",
                 swagger.fullname, swagger.id, notif.status);
      ELLE_ASSERT(notif.status == gap_user_status_online ||
                  notif.status == gap_user_status_offline);

      State::User user = this->_users.at(this->_user_indexes.at(swagger.id));

      if (notif.device_status)
      {
        auto it = std::find(std::begin(user.connected_devices),
                            std::end(user.connected_devices),
                            notif.device_id);

        if (it == std::end(user.connected_devices))
        {
          user.connected_devices.push_back(notif.device_id);
        }
        else
        {
          ELLE_WARN("%s: device %s was already in connected devices",
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
          user.connected_devices.erase(it);
        }
        else
        {
          ELLE_WARN("%s: device %s status wasn't in connected_devices",
                    *this, notif.device_id);
        }
      }

      ELLE_DEBUG("%s: device %s status is %s",
                 *this, notif.device_id, notif.device_status);

      this->enqueue<UserStatusNotification>(
        UserStatusNotification(
          this->_user_indexes.at(swagger.id), notif.status));
    }

    void
    State::swaggers_dirty()
    {
      ELLE_TRACE_METHOD("");

      this->_swaggers_dirty = true;
    }
  }
}
