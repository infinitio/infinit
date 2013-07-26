#ifndef NETWORKMANAGER_HH
# define NETWORKMANAGER_HH

# include "Exception.hh"
# include "NotificationManager.hh"

# include <plasma/meta/Client.hh>

# include <elle/Printable.hh>
# include <elle/threading/Monitor.hh>

namespace surface
{
  namespace gap
  {
    /*-------.
    | Usings |
    `-------*/
    using Network = ::plasma::meta::NetworkResponse;

    class TransactionManager;

    class NetworkManager:
      public Notifiable,
      public elle::Printable
    {
      /*-----------------.
      | Module Exception |
      `-----------------*/
      class Exception: public surface::gap::Exception
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
      |  Attributes |
      `-----------*/
      ELLE_ATTRIBUTE(plasma::meta::Client const&, meta);

      /*-------------.
      | Construction |
      `-------------*/
    public:
      NetworkManager(surface::gap::NotificationManager& notification_manager,
                     plasma::meta::Client const& meta);

      virtual
      ~NetworkManager();

      /*------------.
      |  Attributes |
      `------------*/
    protected:
      typedef std::map<std::string, Network> NetworkMap;
      typedef std::unique_ptr<NetworkMap> NetworkMapPtr;
      typedef elle::threading::Monitor<NetworkMapPtr> NetworkMapMonitor;
    protected:
      NetworkMapMonitor _networks;

    public:
      /// Retrieve all network ids.
      std::vector<std::string>
      all_ids();

      /// Retrieve all networks.
      NetworkMap const&
      all();

      /// Retrieve a network.
      Network
      one(std::string const& id);

      Network
      sync(std::string const& id);

    private:
      /// On NetworkUpdate.
      void
      _on_network_update(NetworkUpdateNotification const& notif);

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
