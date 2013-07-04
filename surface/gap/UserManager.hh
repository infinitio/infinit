#ifndef USERMANAGER_HH
# define USERMANAGER_HH

# include <surface/gap/Exception.hh>
# include <surface/gap/NotificationManager.hh>
# include <surface/gap/usings.hh>
# include <plasma/meta/Client.hh>

# include <elle/Printable.hh>

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
      public Notifiable,
      public elle::Printable
    {
      /*----------.
      | Exception |
      `----------*/
    public:
      class Exception:
        surface::gap::Exception
      {
      public:
        Exception(gap_Status error, std::string const& what):
          surface::gap::Exception{error, what}
        {}

        Exception(std::string const& what):
          surface::gap::Exception{gap_error, what}
        {}
      };

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
      std::map<std::string, User*> _users;
      std::unordered_set<std::string> _connected_devices;

      /// XXX.
      void
      _on_resync();

      /// Update the current user container with the current meta user data.
      User const&
      _sync(plasma::meta::UserResponse const& res);

      /*-------.
      | Access |
      `-------*/
    public:
      /// Force the update of an user.
      User const&
      sync(std::string const& id);

      /// Return the cached version of a user if it exists. If not, sync the
      /// user from meta. You can retrieve it using id or email but only the id
      /// version use caching.
      User const&
      one(std::string const& id);

      /// Return the cached version of a user if it exists according to it
      /// public key. If not, sync the user from meta.
      User const&
      from_public_key(std::string const& public_key);

      /// Search users
      std::map<std::string, User const*>
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
      typedef std::unordered_set<std::string> SwaggersSet;
      SwaggersSet _swaggers;
      bool _swaggers_dirty;

      /*-------.
      | Access |
      `-------*/
    public:
      SwaggersSet const&
      swaggers();

      User const&
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

    /*----------.
    | Printable |
    `----------*/
    public:
      virtual
      void
      print(std::ostream& stream) const override;
    };
  }
}

#endif
