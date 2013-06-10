#ifndef NETWORKMANAGER_HH
# define NETWORKMANAGER_HH

# include <surface/gap/NotificationManager.hh>
# include <surface/gap/Exception.hh>
# include <surface/gap/InfinitInstanceManager.hh>
# include <surface/gap/metrics.hh>

# include <nucleus/neutron/Permissions.hh>

# include <plasma/meta/Client.hh>

# include <reactor/scheduler.hh>

# include <elle/threading/Monitor.hh>

namespace surface
{
  namespace gap
  {
    /*-------.
    | Usings |
    `-------*/
    using Self = ::plasma::meta::SelfResponse;
    using Network = ::plasma::meta::NetworkResponse;
    using Device = ::plasma::meta::Device;
    using Endpoint = ::plasma::meta::EndpointNodeResponse;

    class NetworkManager
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
      | Attributes |
      `-----------*/
    private:
      // XXX: meta should be constant everywhere.
      // But httpclient fire can't be constant.
      plasma::meta::Client& _meta;
      elle::metrics::Reporter& _reporter;
      elle::metrics::Reporter& _google_reporter;
      Self const& _self;
      Device const& _device;
      ELLE_ATTRIBUTE_R(InfinitInstanceManager, infinit_instance_manager);

      /*-------------.
      | Construction |
      `-------------*/
    public:
      NetworkManager(plasma::meta::Client& meta,
                     elle::metrics::Reporter& reporter,
                     elle::metrics::Reporter& google_reporter,
                     Self const& me,
                     Device const& device);

      virtual
      ~NetworkManager();

      void
      clear();

    public:
      void
      wait_portal(std::string const& network_id);


      /*------------.
      |  Attributes |
      `------------*/
    protected:
      typedef std::unique_ptr<Network> NetworkPtr;
      typedef std::map<std::string, NetworkPtr> NetworkMap;
      typedef std::unique_ptr<NetworkMap> NetworkMapPtr;
      typedef elle::threading::Monitor<NetworkMapPtr> NetworkMapMonitor;
    protected:
      NetworkMapMonitor _networks;


    protected:
      /// Retreive all networks, ensuring the map is initialized.
      NetworkMapMonitor&
      _all();

    public:
      /// Retrieve all networks.
      std::vector<std::string>
      all_ids();

      /// Retrieve a network.
      Network const&
      one(std::string const& id);

      Network const&
      sync(std::string const& id);

      /// Create a new network.
      std::string
      create(std::string const& name,
             bool auto_add = true);

      /// Prepare directories and files for the network to be launched.
      void
      prepare(std::string const& network_id);

      /// Delete a new network.
      std::string
      delete_(std::string const& name,
              bool force = false);

      /// Add a user to a network with its mail or id.
      void
      add_user(std::string const& network_id,
               std::string const& inviter_id,
               std::string const& user_id,
               std::string const& identity);

      /// Add a device to a network.
      void
      add_device(std::string const& network_id,
                 std::string const& device_id);

      /// Connect 2 devices via infinit.
      void
      notify_8infinit(std::string const& network_id,
                      std::string const& sender_device_id,
                      std::string const& recipient_device_id);
      void
      _notify_8infinit(std::string const& network_id,
                       std::string const& sender_device_id,
                       std::string const& recipient_device_id,
                       reactor::Scheduler& sched);

      /// Give the recipient the write on the root of the network.
      void
      set_permissions(std::string const& network_id,
                      std::string const& user_id,
                      std::string const& user_identity,
                      nucleus::neutron::Permissions permissions);

      ///
      void
      to_directory(std::string const& network_id,
                   std::string const& path);

    private:
      /// On NetworkUpdate.
      void
      _on_network_update(NetworkUpdateNotification const& notif);

    };
  }
}

#endif
