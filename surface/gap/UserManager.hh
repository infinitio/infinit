#ifndef USERMANAGER_HH
# define USERMANAGER_HH

# include <surface/gap/Exception.hh>
# include <surface/gap/NotificationManager.hh>
# include <surface/gap/usings.hh>
# include <plasma/meta/Client.hh>

# include <elle/threading/Monitor.hh>

# include <unordered_set>

namespace surface
{
  namespace gap
  {
    using User = ::plasma::meta::User;
    using Self = ::plasma::meta::SelfResponse;
    using NotifManager = ::surface::gap::NotificationManager;
    using UserStatusNotification = ::surface::gap::UserStatusNotification;
    using NewSwaggerNotification = ::surface::gap::NewSwaggerNotification;

    class UserManager:
      public Notifiable
    {
      /*-----------.
      | Attributes |
      `-----------*/
    private:
      plasma::meta::Client& _meta;
      Self const& _self;

      /*-------------.
      | Construction |
      `-------------*/
    public:
      UserManager(NotifManager& notification_manager,
                  plasma::meta::Client& meta,
                  Self const& self);

      virtual
      ~UserManager();

      /*------.
      | Cache |
      `------*/
    private:
      typedef std::unique_ptr<User> UserPtr;
      // XXX pointers are not needed anymore.
      typedef std::map<std::string, UserPtr> UserMap;
      elle::threading::Monitor<UserMap> _users;

      // Hooked to the notification manager, resync users on reconnection.
      void
      _on_resync();

      // Update the current user container with the current meta user data.
      User
      _sync(plasma::meta::UserResponse const& res);

      // Force the update of an user.
      User
      _sync(std::string const& id);

      /*-------.
      | Access |
      `-------*/
    public:

      /// Return the cached version of an user if it exists. If not, sync the
      /// user from meta. You can retrieve it using id or email but only the id
      /// version use caching.
      User
      one(std::string const& id);

      /// Return the cached version of an user if it exists according to its
      /// public key. If not, sync the user from meta.
      User
      from_public_key(std::string const& public_key);

      /// Search users
      std::map<std::string, User>
      search(std::string const& text);

      /// Device connection status
      bool
      device_status(std::string const& user_id,
                    std::string const& device_id);

      elle::Buffer
      icon(std::string const& id);

      /*-------------.
      | Interraction |
      `-------------*/
      std::string
      invite(std::string const& email);

      /// Send message to user @id via trophonius
      void
      send_message(std::string const& recipient_id,
                   std::string const& message);

      //- Swaggers -------------------------------------------------------------
      /*--------.
      | Storage |
      `--------*/
    private:
      typedef std::unordered_set<std::string> SwaggerSet;
      elle::threading::Monitor<SwaggerSet> _swaggers;
      bool _swaggers_dirty;

      /*-------.
      | Access |
      `-------*/
    public:
      SwaggerSet
      swaggers();

      User
      swagger(std::string const& id);

     void
     swaggers_dirty();
      /*----------.
      | Callbacks |
      `----------*/
    public:
      void
      _on_new_swagger(NewSwaggerNotification const& notification);

      void
      _on_swagger_status_update(UserStatusNotification const& notif);

    };
  }
}

#endif
