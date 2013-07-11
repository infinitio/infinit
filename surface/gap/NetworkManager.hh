#ifndef NETWORKMANAGER_HH
# define NETWORKMANAGER_HH

# include "Device.hh"
# include "Exception.hh"
# include "InfinitInstanceManager.hh"
# include "NotificationManager.hh"
# include "Self.hh"
# include "metrics.hh"
# include "Rounds.hh"

# include <nucleus/neutron/Permissions.hh>

# include <plasma/meta/Client.hh>

# include <reactor/scheduler.hh>

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
    using Endpoint = ::plasma::meta::EndpointNodeResponse;

    class NetworkManager:
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
      | Attributes |
      `-----------*/
    private:
      // XXX: meta should be constant everywhere.
      // But httpclient fire can't be constant.
      plasma::meta::Client& _meta;
      elle::metrics::Reporter& _reporter;
      elle::metrics::Reporter& _google_reporter;
      typedef std::function<Self const&()> SelfGetter;
      typedef std::function<Device const&()> DeviceGetter;
      ELLE_ATTRIBUTE(SelfGetter, self);
      ELLE_ATTRIBUTE(DeviceGetter, device);
      ELLE_ATTRIBUTE_X(InfinitInstanceManager, infinit_instance_manager);

      /*-------------.
      | Construction |
      `-------------*/
    public:
      NetworkManager(plasma::meta::Client& meta,
                     elle::metrics::Reporter& reporter,
                     elle::metrics::Reporter& google_reporter,
                     SelfGetter const& me,
                     DeviceGetter const& device);

      virtual
      ~NetworkManager();

      void
      clear();

    public:
      void
      launch(std::string const& network_id);


      /*------------.
      |  Attributes |
      `------------*/
    protected:
      typedef std::map<std::string, Network> NetworkMap;
      typedef elle::threading::Monitor<NetworkMap> NetworkMapMonitor;
    protected:
      NetworkMapMonitor _networks;

    public:
      /// Retrieve all networks.
      std::vector<std::string>
      all_ids();

      /// Retrieve a network.
      Network
      one(std::string const& id);

      Network
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
              bool remove_directory = true);

      /// Remove local directories (and kill any infinit instances).
      void
      delete_local(std::string const& name);

      /// Add a user to a network with its mail or id.
      void
      add_user(std::string const& network_id,
               std::string const& user_K);

      /// Upload files (wrap instance_manager.upload_files)
      void
      upload_files(std::string const& network_id,
                   std::unordered_set<std::string> const& files,
                   std::function<void ()> success_callback,
                   std::function<void ()> failure_callback);


      /// Download files into path 'destination' (wrap).
      void
      download_files(std::string const& network_id,
                     std::vector<round> const& addresses,
                     std::string const& public_key,
                     std::string const& destination,
                     std::function<void ()> success_callback,
                     std::function<void ()> failure_callback);

      /// Get the progress on the current network.
      float
      progress(std::string const& network_id);

      /// Add a device to a network.
      void
      add_device(std::string const& network_id,
                 std::string const& device_id);

      /// Get peer addresses.
      /// Return a list of rounds.
      std::vector<round>
      peer_addresses(std::string const& network_id,
                     std::string const& sender_device_id,
                     std::string const& recipient_device_id);

      /// Give the recipient the write on the root of the network.
      void
      set_permissions(std::string const& network_id,
                      std::string const& peer_pu);
      ///
      void
      to_directory(std::string const& network_id,
                   std::string const& path);

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
