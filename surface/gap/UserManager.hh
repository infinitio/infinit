#ifndef USERMANAGER_HH
# define USERMANAGER_HH

# include <surface/gap/Exception.hh>
# include <surface/gap/NotificationManager.hh>
# include <surface/gap/usings.hh>
# include <plasma/meta/Client.hh>

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
    class UserManager: Notifiable
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

      void
      _on_resync();

      /*-------.
      | Access |
      `-------*/
    public:
      /// Retrieve a user by id or with its email.
      User const&
      one(std::string const& id);

      /// Retrieve a user by its public key.
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

    };
  }
}

#endif
