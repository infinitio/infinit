#ifndef USERMANAGER_HH
# define USERMANAGER_HH

# include <surface/gap/Exception.hh>
# include <surface/gap/NotificationManager.hh>
# include <plasma/meta/Client.hh>

namespace surface
{
  namespace gap
  {
    using User = ::plasma::meta::User;
    using Self = ::plasma::meta::SelfResponse;
    using NotifManager = ::surface::gap::NotificationManager;
    class UserManager: Notifiable
    {
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

    private:
      plasma::meta::Client& _meta;
      Self const& _self;

    public:
      UserManager(NotifManager& notification_manager,
                  plasma::meta::Client& meta,
                  Self const& self);

      // Caching.
    private:
      std::map<std::string, User*> _users;

    public:
      /// Retrieve a user by id or with its email.
      User const&
      one(std::string const& id);

      /// Retrieve a user by its public key.
      User const&
      from_public_key(std::string const& public_key);

      // Search users
      std::map<std::string, User const*>
      search(std::string const& text);

      elle::Buffer
      icon(std::string const& id);

      std::string
      invite(std::string const& email);

      /// Send message to user @id via trophonius
      void
      send_message(std::string const& recipient_id,
                   std::string const& message);

    public:
      void
      _on_swagger_status_update(plasma::trophonius::UserStatusNotification const& notif);

      /// Swaggers.
    private:
      typedef std::map<std::string, User const*> SwaggersMap;
      SwaggersMap _swaggers;
      bool _swaggers_dirty;

    public:
      SwaggersMap const&
      swaggers();

      User const&
      swagger(std::string const& id);

     void
     swaggers_dirty();
    };
  }
}

#endif
